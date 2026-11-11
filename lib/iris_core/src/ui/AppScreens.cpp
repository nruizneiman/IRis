#include "ui/AppScreens.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

#include "app/BatteryLevel.h"
#include "app/LibraryExport.h"
#include "app/RemoteView.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"
#include "domain/Naming.h"
#include "domain/SignalValidation.h"
#include "ui/I18n.h"
#include "ui/ModuleScreens.h"
#include "ui/ScreenKit.h"
#include "ui/Icons.h"
#include "ui/Utf8.h"
#include "ui/Widgets.h"

namespace iris {

namespace {

// Typical names offered before the keyboard so most entries need no typing. Each fits kMaxNameBytes.
std::vector<std::string> buttonPresets() {
  return {"Encender", "Apagar",   "Subir vol.", "Bajar vol.", "Silenciar", "Canal +",  "Canal -", "Menú",
          "Aceptar",  "Arriba",   "Abajo",      "Izquierda",  "Derecha",   "Volver",   "Entrada", "Inicio"};
}
std::vector<std::string> categoryPresets() {
  return {"TV", "Aire acond.", "Luces", "Audio", "Ventilador", "Proyector", "Decodificador", "Varios"};
}
std::vector<std::string> remotePresets() {
  return {"Televisor", "Aire", "Tira LED", "Ventilador", "Parlante", "Proyector", "Decodificador"};
}
std::unique_ptr<TextInputScreen> withPresets(std::unique_ptr<TextInputScreen> screen, std::vector<std::string> list) {
  screen->presets(std::move(list));
  return screen;
}


using P = std::unique_ptr<Screen>;

// ---------------------------------------------------------------- helpers

std::string hexStr(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%lX", static_cast<unsigned long>(v));
  return b;
}

std::string numStr(unsigned long long v) { return std::to_string(v); }

const char* recognitionEs(Recognition r) {
  switch (r) {
    case Recognition::Recognized: return "Reconocida";
    case Recognition::Partial: return "Parcial";
    case Recognition::Unknown: return "Desconocida";
    case Recognition::Invalid: return "Inválida";
  }
  return "?";
}

std::string shortSignal(const IrSignal& s) {
  if (!s.initial.empty() && s.initial[0].kind == Frame::Kind::Decoded) {
    const ProtocolInfo* p = findProtocol(s.initial[0].decoded.protocol);
    return p ? p->displayName : "?";
  }
  return "RAW";
}

std::vector<std::string> signalLines(const IrSignal& s) {
  std::vector<std::string> out;
  out.push_back(std::string(recognitionEs(s.recognition)));
  if (!s.initial.empty() && s.initial[0].kind == Frame::Kind::Decoded) {
    const DecodedFrame& d = s.initial[0].decoded;
    const ProtocolInfo* p = findProtocol(d.protocol);
    out.push_back(std::string(p ? p->displayName : "?") + " " + numStr(d.bits) + " bits");
    out.push_back("Dir " + hexStr(d.address) + " Cmd " + hexStr(d.command));
  } else if (!s.initial.empty()) {
    out.push_back("RAW: " + numStr(s.initial[0].raw.durationsUs.size()) + " pulsos");
    out.push_back(s.initial[0].raw.carrierHz ? "Portadora " + numStr(s.initial[0].raw.carrierHz) + " Hz"
                                             : std::string("Portadora desconocida"));
  }
  if (s.quality.possibleNoise) out.push_back("¡Posible ruido!");
  if (s.quality.truncated) out.push_back("¡Captura truncada!");
  if (s.originalCapture) out.push_back("RAW original: " + numStr(s.originalCapture->durationsUs.size()));
  return out;
}

// Reports a failed library operation; on success persists and shows `okMsg`. Returns op success.
bool finishChange(UiContext& c, const Status& st, const std::string& okMsg) {
  if (!st.ok()) {
    c.toast(spanishError(st));
    return false;
  }
  Status saved = c.persist();
  if (!saved.ok()) {
    // The change stays in memory and the library remains marked dirty (the home screen shows "*").
    c.toast(saved.code == Err::Unavailable ? "Guardado desactivado"
            : saved.code == Err::LimitReached ? "Memoria insuficiente"
                                              : "¡No se pudo guardar!",
            3000);
    return true;
  }
  c.toast(okMsg);
  return true;
}

void sendSignal(UiContext& c, const IrSignal& s) {
  Status st = c.transmit.send(s);
  c.toast(st.ok() ? std::string("Enviando...") : (st.code == Err::Unavailable ? "Ocupado, espere" : spanishError(st)));
}

void sendButton(UiContext& c, Id remote, Id button) {
  const Button* b = c.library.findButton(remote, button);
  if (!b) return c.toast("Botón no encontrado");
  if (!b->signal) return c.toast("Sin señal guardada");
  sendSignal(c, *b->signal);
}

// Writes controls to the card with `doExport` and says how it went. Nothing is written unless the user asked for it.
void runExport(UiContext& c, const std::function<Status(IFileSink&, ExportSummary&)>& doExport) {
  if (!c.sink || !c.sink->writable()) return c.toast("Sin tarjeta SD");
  ExportSummary summary;
  const Status st = doExport(*c.sink, summary);
  if (summary.skippedButtons > 0) {
    c.log.log(LogLevel::Info, "export", numStr(summary.skippedButtons) + " buttons not written (empty or unsupported)");
  }
  if (!st.ok()) return c.toast(st.code == Err::Unavailable ? "Sin tarjeta SD" : "No se pudo escribir", 3000);
  if (summary.remotes == 0) return c.toast("Sin nada que exportar");
  c.toast("Exportados: " + numStr(summary.remotes));
}

IrSignal rawReplay(const IrSignal& captured) {
  IrSignal s;
  s.provenance = Provenance::Learned;
  s.recognition = Recognition::Unknown;
  RawFrame raw = captured.originalCapture ? *captured.originalCapture : RawFrame{};
  if (raw.durationsUs.empty() && !captured.initial.empty() && captured.initial[0].kind == Frame::Kind::Raw) {
    raw = captured.initial[0].raw;
  }
  s.initial.push_back(Frame::makeRaw(std::move(raw)));
  return s;
}

std::string remoteName(UiContext& c, Id id) {
  const Remote* r = c.library.findRemote(id);
  return r ? r->name : "?";
}

std::string buttonName(UiContext& c, Id remote, Id button) {
  const Button* b = c.library.findButton(remote, button);
  return b ? b->name : "?";
}

std::string batteryText(UiContext& c) {
  auto mv = c.battery.readMillivolts();
  if (!mv) return "Bat n/d";
  char b[16];
  std::snprintf(b, sizeof b, "%u.%02u V", static_cast<unsigned>(*mv / 1000), static_cast<unsigned>((*mv % 1000) / 10));
  return b;
}

const char* keyEs(Key k) {
  switch (k) {
    case Key::Up: return "Arriba";
    case Key::Down: return "Abajo";
    case Key::Left: return "Izquierda";
    case Key::Right: return "Derecha";
    case Key::Ok: return "OK";
  }
  return "?";
}

// Forward declarations (screens reference each other).
P makeLibraryScreen();
P makeCategoryScreen(Id cat);
P makeRemoteScreen(Id remote);
P makeButtonMenu(Id remote, Id button);
P makeDiagnosticsScreen();

// Picks a destination button. `extraPops` is 1 when a text editor is on top of the picker (new button).
using PickFn = std::function<Nav(UiContext&, Id remote, Id button, int extraPops)>;
P makeRemotePicker(const std::string& title, PickFn onPick);
P makeButtonPicker(Id remote, PickFn onPick);

// ---------------------------------------------------------------- diagnostics / info pages

std::vector<std::string> diagnosticsLines(UiContext& c) {
  std::vector<std::string> l;
  l.push_back(std::string("Pantalla: ") + (c.displayHealthy ? "OK" : "FALLA"));
  l.push_back("Ctrl: " + c.info.displayController);
  l.push_back(std::string("Receptor: ") + (c.receiver.running() ? "activo" : "detenido"));
  const char* tx = "reposo";
  switch (c.transmit.state()) {
    case TxState::Idle: tx = "reposo"; break;
    case TxState::Sending: tx = "enviando"; break;
    case TxState::Cancelling: tx = "cancelando"; break;
    case TxState::Failed: tx = "FALLA"; break;
  }
  l.push_back(std::string("Emisor: ") + tx);
  {
    const auto mv = c.battery.readMillivolts();
    l.push_back("Batería: " + (mv ? batteryText(c) : std::string("n/d")));
    if (mv) l.push_back("Nivel est.: ~" + std::to_string(batteryPercentApprox(batteryCellMillivolts(*mv))) + "%");
  }
  l.push_back("Reinicio: " + c.info.resetReason);
  l.push_back("Almacén: " + c.info.storage);
  l.push_back(std::string("Sin guardar: ") + (c.library.dirty() ? "SÍ" : "no"));
  if (c.info.adapterCounters) {
    for (const std::string& line : c.info.adapterCounters()) l.push_back(line);
  }
  const auto& e = c.log.entries();
  const size_t n = e.size() < 4 ? e.size() : 4;
  for (size_t i = e.size() - n; i < e.size(); ++i) l.push_back("[" + e[i].tag + "] " + e[i].message);
  return l;
}

P makeDiagnosticsScreen() { return std::make_unique<InfoScreen>("diagnostics", "Diagnóstico", diagnosticsLines); }

// ---------------------------------------------------------------- learn / raw capture

class LearnScreen : public Screen {
 public:
  LearnScreen(Id remote, Id button, int returnPops) : remote_(remote), button_(button), pops_(returnPops) {}

  const char* id() const override { return "learn"; }
  void onEnter(UiContext& c) override { begin(c); }
  void onExit(UiContext& c) override { c.learn.cancel(); }

  Nav onKey(UiContext& c, const KeyEvent& e) override {
    const bool press = e.action == KeyAction::Press;
    const LearnState st = c.learn.state();
    if (st == LearnState::Captured) {
      const size_t n = kActionCount;
      if ((e.key == Key::Up || e.key == Key::Down) && e.action != KeyAction::LongPress) {
        sel_ = (e.key == Key::Down) ? (sel_ + 1) % n : (sel_ + n - 1) % n;
        return Nav::none();
      }
      if (press && e.key == Key::Ok) return runAction(c);
    } else if (st != LearnState::Armed && press && e.key == Key::Ok) {
      begin(c);  // retry after a timeout / rejection / receiver failure
      return Nav::none();
    }
    if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::home();
    if (e.key == Key::Left && press) return Nav::pop();
    return Nav::none();
  }

  void render(UiContext& c, Painter& p) override {
    p.title("Aprender: " + buttonName(c, remote_, button_));
    switch (c.learn.state()) {
      case LearnState::Armed: {
        const uint32_t elapsed = c.clock.nowMs() - startMs_;
        const uint32_t total = static_cast<uint32_t>(c.settings.captureTimeoutSec) * 1000u;
        p.text(0, 2, "Esperando señal...");
        p.text(0, 3, "Apunte el control al");
        p.text(0, 4, "receptor y pulse.");
        p.text(0, 5, "Tiempo: " + numStr(elapsed >= total ? 0 : (total - elapsed + 999) / 1000) + " s");
        p.footer("Izq:cancelar");
        break;
      }
      case LearnState::Captured: {
        const IrSignal& got = *c.learn.result();
        std::vector<std::string> lines = signalLines(got);
        if (got.quality.possibleNoise) lines[0] += " (ruido)";
        if (got.quality.truncated) lines[0] += " (trunc.)";
        for (size_t i = 0; i < lines.size() && i < 3; ++i) p.text(0, 1 + static_cast<int>(i), lines[i]);
        static const char* const names[kActionCount] = {"Guardar", "Probar envío", "Reintentar"};
        for (size_t i = 0; i < kActionCount; ++i) p.item(4 + static_cast<int>(i), names[i], "", i == sel_);
        p.footer("OK:elegir Izq:salir");
        break;
      }
      case LearnState::Rejected:
        p.text(0, 2, "Captura inválida:");
        p.text(0, 3, "vacía o muy corta.");
        p.footer("OK:reintentar Izq:salir");
        break;
      case LearnState::TimedOut:
        p.text(0, 2, "No llegó ninguna");
        p.text(0, 3, "señal a tiempo.");
        p.footer("OK:reintentar Izq:salir");
        break;
      case LearnState::Failed:
        p.text(0, 2, "Falló el receptor IR");
        p.text(0, 3, spanishError(c.learn.lastStatus()));
        p.footer("OK:reintentar Izq:salir");
        break;
      case LearnState::Idle:
        p.text(0, 3, "Preparando...");
        p.footer("Izq:salir");
        break;
    }
  }

 private:
  void begin(UiContext& c) {
    sel_ = 0;
    startMs_ = c.clock.nowMs();
    Status st = c.learn.start(static_cast<uint32_t>(c.settings.captureTimeoutSec) * 1000u);
    if (!st.ok()) c.toast(spanishError(st));
  }
  static constexpr size_t kActionCount = 3;
  Nav runAction(UiContext& c) {
    const IrSignal& sig = *c.learn.result();
    if (sel_ == 0) {
      Status st = c.library.setButtonSignal(remote_, button_, sig);
      if (!finishChange(c, st, "Señal guardada")) return Nav::none();
      c.learn.take();
      return Nav::pop(pops_);
    }
    if (sel_ == 1) sendSignal(c, sig);
    else begin(c);  // "Reintentar"
    return Nav::none();
  }

  Id remote_, button_;
  int pops_;
  size_t sel_ = 0;
  uint32_t startMs_ = 0;
};

// ---------------------------------------------------------------- manual signal creation

// The manual-signal and exploration forms both start with "Protocolo" (field 0) and "Bits" (field 1).
std::vector<std::string> protocolNames() {
  std::vector<std::string> names;
  for (size_t i = 0; i < protocolCount(); ++i) names.push_back(protocolAt(i)->displayName);
  return names;
}

// Rebuilds the bit-count options when the protocol changed and returns the chosen variant (null: no protocol).
const ProtocolVariant* syncProtocolFields(FormScreen& f, size_t changed) {
  const ProtocolInfo* info = protocolAt(f.field(0).choice);
  if (!info) return nullptr;
  if (changed == 0) {
    f.field(1).options.clear();
    for (uint8_t i = 0; i < info->variantCount; ++i) f.field(1).options.push_back(numStr(info->variants[i].bits));
    f.field(1).choice = 0;
  }
  return &info->variants[f.field(1).choice];
}

void limitField(FormField& field, uint32_t max) {
  field.max = max;
  if (field.number > max) field.number = max;
}

void syncManualForm(FormScreen& f, size_t changed) {
  const ProtocolVariant* v = syncProtocolFields(f, changed);
  if (!v) return;
  const ProtocolInfo* info = protocolAt(f.field(0).choice);
  limitField(f.field(2), maskForBits(v->addressBits));
  limitField(f.field(3), maskForBits(v->commandBits));
  f.field(4).visible = info->hasToggle;
  if (!info->hasToggle) f.field(4).choice = 0;
}

Result<IrSignal> manualFromForm(const FormScreen& f) {
  const ProtocolInfo* info = protocolAt(f.field(0).choice);
  if (!info) return Status::fail(Err::InvalidArgument, "no protocol");
  const uint16_t bits = info->variants[f.field(1).choice].bits;
  return buildDecodedSignal(info->id, f.field(2).number, f.field(3).number, bits, f.field(4).choice == 1);
}

P makeManualForm(Id remote, Id button) {
  auto form = std::make_unique<FormScreen>("manual-signal", "Señal manual");
  form->addChoice("Protocolo", protocolNames());
  form->addChoice("Bits", {"32"});
  form->addNumber("Dirección", 0, 0, 0xFF, 16);
  form->addNumber("Comando", 0, 0, 0xFF, 16);
  form->addChoice("Toggle", {"No", "Sí"});
  FormScreen* raw = form.get();
  form->addInfo([raw]() {
    auto r = manualFromForm(*raw);
    if (!r.ok()) return std::string("Vista previa: inválida");
    const DecodedFrame& d = r.value().initial[0].decoded;
    return shortSignal(r.value()) + " " + numStr(d.bits) + "b " + hexStr(d.address) + "/" + hexStr(d.command);
  });
  form->addAction("Probar envío", [](UiContext& c, FormScreen& f) {
    auto r = manualFromForm(f);
    if (!r.ok()) c.toast(spanishError(r.status()));
    else sendSignal(c, r.value());
    return Nav::none();
  });
  form->addAction("Guardar", [remote, button](UiContext& c, FormScreen& f) {
    auto r = manualFromForm(f);
    if (!r.ok()) {
      c.toast(spanishError(r.status()));
      return Nav::none();
    }
    Status st = c.library.setButtonSignal(remote, button, r.value());
    return finishChange(c, st, "Señal guardada") ? Nav::pop(2) : Nav::none();
  });
  form->onChange(syncManualForm);
  syncManualForm(*form, 0);
  return form;
}

// ---------------------------------------------------------------- library screens

P makeButtonMenu(Id remote, Id button) {
  auto menu = std::make_unique<ListScreen>(
      "button-menu", "Botón", [remote, button](UiContext& c) {
        std::vector<ListItem> it;
        it.push_back({"Enviar", "", [remote, button](UiContext& cc) { sendButton(cc, remote, button); return Nav::none(); }});
        it.push_back({"Aprender señal", "", [remote, button](UiContext&) {
                        return Nav::push(std::make_unique<LearnScreen>(remote, button, 2));
                      }});
        it.push_back({"Crear manual", "", [remote, button](UiContext&) { return Nav::push(makeManualForm(remote, button)); }});
        it.push_back({"Renombrar", "", [remote, button](UiContext& cc) {
                        return Nav::push(std::make_unique<TextInputScreen>(
                            "Renombrar botón", buttonName(cc, remote, button), limits::kMaxNameBytes,
                            [remote, button](UiContext& c2, const std::string& name) {
                              Status st = c2.library.renameButton(remote, button, name);
                              return finishChange(c2, st, "Renombrado") ? Nav::pop(2) : Nav::none();
                            }));
                      }});
        it.push_back({"Borrar señal", "", [remote, button](UiContext& cc) {
                        return Nav::push(std::make_unique<ConfirmScreen>(
                            "¿Borrar señal?", std::vector<std::string>{buttonName(cc, remote, button)},
                            [remote, button](UiContext& c2) {
                              finishChange(c2, c2.library.clearButtonSignal(remote, button), "Señal borrada");
                              return Nav::pop(2);
                            }));
                      }});
        it.push_back({"Eliminar botón", "", [remote, button](UiContext& cc) {
                        return Nav::push(std::make_unique<ConfirmScreen>(
                            "¿Eliminar botón?", std::vector<std::string>{buttonName(cc, remote, button)},
                            [remote, button](UiContext& c2) {
                              finishChange(c2, c2.library.removeButton(remote, button), "Botón eliminado");
                              return Nav::pop(2);
                            }));
                      }});
        (void)c;
        return it;
      });
  menu->titleFn([remote, button](UiContext& c) { return "Botón " + buttonName(c, remote, button); });
  return menu;
}

P makeRemoteMenu(Id remote) {
  auto menu = std::make_unique<ListScreen>("remote-menu", "Control", [remote](UiContext&) {
    std::vector<ListItem> it;
    it.push_back({"Renombrar", "", [remote](UiContext& c) {
                    return Nav::push(std::make_unique<TextInputScreen>(
                        "Renombrar control", remoteName(c, remote), limits::kMaxNameBytes,
                        [remote](UiContext& c2, const std::string& name) {
                          Status st = c2.library.renameRemote(remote, name);
                          return finishChange(c2, st, "Renombrado") ? Nav::pop(2) : Nav::none();
                        }));
                  }});
    it.push_back({"Cambiar categoría", "", [remote](UiContext&) {
                    return Nav::push(std::make_unique<ListScreen>(
                        "move-remote", "Mover a...", [remote](UiContext& c) {
                          std::vector<ListItem> items;
                          for (const Category& cat : c.library.data().categories) {
                            const Id catId = cat.id;
                            items.push_back({cat.name, "", [remote, catId](UiContext& c2) {
                                               Status st = c2.library.moveRemote(remote, catId);
                                               return finishChange(c2, st, "Movido") ? Nav::pop(2) : Nav::none();
                                             }});
                          }
                          return items;
                        }));
                  }});
    it.push_back({"Duplicar", "", [remote](UiContext& c) {
                    std::string initial = remoteName(c, remote);
                    while (initial.size() + 2 > limits::kMaxNameBytes && !initial.empty()) {
                      // drop whole UTF-8 characters so the copy's name stays valid
                      while (!initial.empty() && (static_cast<unsigned char>(initial.back()) & 0xC0) == 0x80) initial.pop_back();
                      if (!initial.empty()) initial.pop_back();
                    }
                    return Nav::push(std::make_unique<TextInputScreen>(
                        "Nombre de la copia", initial + " 2", limits::kMaxNameBytes,
                        [remote](UiContext& c2, const std::string& name) {
                          auto copy = c2.library.duplicateRemote(remote, name);
                          bool ok = finishChange(c2, copy.ok() ? Status::success() : copy.status(), "Control duplicado");
                          return ok && copy.ok() ? Nav::pop(2) : Nav::none();
                        }));
                  }});
    it.push_back({"Exportar a SD", "", [remote](UiContext& c) {
                    runExport(c, [&](IFileSink& sink, ExportSummary& s) { return exportRemote(sink, c.library, remote, s); });
                    return Nav::none();
                  }});
    it.push_back({"Modo loop", "", [remote](UiContext&) { return Nav::push(makeLoopSetup(RemoteRef{remote, ""})); }});
    it.push_back({"Modo loco", "", [remote](UiContext&) { return Nav::push(makeCrazySetup(RemoteRef{remote, ""})); }});
    it.push_back({"Eliminar control", "", [remote](UiContext& c) {
                    const Remote* r = c.library.findRemote(remote);
                    return Nav::push(std::make_unique<ConfirmScreen>(
                        "¿Eliminar control?",
                        std::vector<std::string>{remoteName(c, remote), "y sus " + numStr(r ? r->buttons.size() : 0) + " botones"},
                        [remote](UiContext& c2) {
                          finishChange(c2, c2.library.removeRemote(remote), "Control eliminado");
                          return Nav::pop(2);
                        }));
                  }});
    return it;
  });
  menu->titleFn([remote](UiContext& c) { return remoteName(c, remote); });
  return menu;
}

P makeRemoteScreen(Id remote) {
  auto list = std::make_unique<ListScreen>("remote", "Control", [remote](UiContext& c) {
    std::vector<ListItem> it;
    if (const Remote* r = c.library.findRemote(remote)) {
      for (const Button& b : r->buttons) {
        const Id bid = b.id;
        it.push_back({b.name, b.signal ? shortSignal(*b.signal) : std::string("vacío"), [remote, bid](UiContext& cc) {
                        const Button* bb = cc.library.findButton(remote, bid);
                        if (bb && bb->signal) {
                          sendButton(cc, remote, bid);
                          return Nav::none();
                        }
                        return Nav::push(makeButtonMenu(remote, bid));
                      }});
      }
    }
    it.push_back({"+ Nuevo botón", "", [remote](UiContext&) {
                    return Nav::push(withPresets(std::make_unique<TextInputScreen>(
                        "Nuevo botón", "", limits::kMaxNameBytes, [remote](UiContext& c2, const std::string& name) {
                          auto r = c2.library.addButton(remote, name);
                          if (!finishChange(c2, r.ok() ? Status::success() : r.status(), "Botón creado")) return Nav::none();
                          return Nav::replace(makeButtonMenu(remote, r.value()));
                        }), buttonPresets()));
                  }});
    return it;
  });
  list->titleFn([remote](UiContext& c) { return remoteName(c, remote); });
  list->footer("OK:enviar Der:menú");
  list->onContext([remote](UiContext& c, size_t idx) {
    const Remote* r = c.library.findRemote(remote);
    if (!r || idx >= r->buttons.size()) return Nav::none();
    return Nav::push(makeButtonMenu(remote, r->buttons[idx].id));
  });
  return list;
}

P makeCategoryMenu(Id cat) {
  auto menu = std::make_unique<ListScreen>("category-menu", "Categoría", [cat](UiContext&) {
    std::vector<ListItem> it;
    it.push_back({"Renombrar", "", [cat](UiContext& c) {
                    const Category* cc = c.library.findCategory(cat);
                    return Nav::push(std::make_unique<TextInputScreen>(
                        "Renombrar categoría", cc ? cc->name : "", limits::kMaxNameBytes,
                        [cat](UiContext& c2, const std::string& name) {
                          Status st = c2.library.renameCategory(cat, name);
                          return finishChange(c2, st, "Renombrada") ? Nav::pop(2) : Nav::none();
                        }));
                  }});
    it.push_back({"Exportar a SD", "", [cat](UiContext& c) {
                    runExport(c, [&](IFileSink& sink, ExportSummary& s) { return exportCategory(sink, c.library, cat, s); });
                    return Nav::none();
                  }});
    it.push_back({"Eliminar categoría", "", [cat](UiContext& c) {
                    const Category* cc = c.library.findCategory(cat);
                    return Nav::push(std::make_unique<ConfirmScreen>(
                        "¿Eliminar categoría?", std::vector<std::string>{cc ? cc->name : "?", "(debe estar vacía)"},
                        [cat](UiContext& c2) {
                          finishChange(c2, c2.library.removeCategory(cat), "Categoría eliminada");
                          return Nav::pop(2);
                        }));
                  }});
    return it;
  });
  menu->titleFn([cat](UiContext& c) {
    const Category* cc = c.library.findCategory(cat);
    return cc ? cc->name : std::string("Categoría");
  });
  return menu;
}

P makeCategoryScreen(Id cat) {
  auto list = std::make_unique<ListScreen>("category", "Categoría", [cat](UiContext& c) {
    std::vector<ListItem> it;
    for (const Remote& r : c.library.data().remotes) {
      if (r.categoryId != cat) continue;
      const Id rid = r.id;
      it.push_back({r.name, numStr(r.buttons.size()) + " bot.", [rid](UiContext&) { return Nav::push(makeRemoteScreen(rid)); }});
    }
    it.push_back({"+ Nuevo control", "", [cat](UiContext&) {
                    return Nav::push(withPresets(std::make_unique<TextInputScreen>(
                        "Nuevo control", "", limits::kMaxNameBytes, [cat](UiContext& c2, const std::string& name) {
                          auto r = c2.library.addRemote(name, cat);
                          if (!finishChange(c2, r.ok() ? Status::success() : r.status(), "Control creado")) return Nav::none();
                          return Nav::replace(makeRemoteScreen(r.value()));
                        }), remotePresets()));
                  }});
    return it;
  });
  list->titleFn([cat](UiContext& c) {
    const Category* cc = c.library.findCategory(cat);
    return cc ? cc->name : std::string("Categoría");
  });
  list->footer("OK:abrir Der:menú");
  list->onContext([cat](UiContext& c, size_t idx) {
    size_t n = 0;
    for (const Remote& r : c.library.data().remotes) {
      if (r.categoryId != cat) continue;
      if (n++ == idx) return Nav::push(makeRemoteMenu(r.id));
    }
    return Nav::none();
  });
  return list;
}

P makeLibraryScreen() {
  auto list = std::make_unique<ListScreen>("library", "Controles", [](UiContext& c) {
    std::vector<ListItem> it;
    for (const Category& cat : c.library.data().categories) {
      size_t n = 0;
      for (const Remote& r : c.library.data().remotes) n += r.categoryId == cat.id ? 1 : 0;
      const Id id = cat.id;
      it.push_back({cat.name, numStr(n), [id](UiContext&) { return Nav::push(makeCategoryScreen(id)); }});
    }
    for (ListItem& card : cardLibraryItems(c)) it.push_back(std::move(card));  // the card's folders, marked "SD"
    it.push_back({"+ Nueva categoría", "", [](UiContext&) {
                    return Nav::push(withPresets(std::make_unique<TextInputScreen>(
                        "Nueva categoría", "", limits::kMaxNameBytes, [](UiContext& c2, const std::string& name) {
                          auto r = c2.library.addCategory(name);
                          return finishChange(c2, r.ok() ? Status::success() : r.status(), "Categoría creada") ? Nav::pop()
                                                                                                                : Nav::none();
                        }), categoryPresets()));
                  }});
    return it;
  });
  list->footer("OK:abrir Der:menú");
  list->onContext([](UiContext& c, size_t idx) {
    if (idx >= c.library.data().categories.size()) return Nav::none();
    return Nav::push(makeCategoryMenu(c.library.data().categories[idx].id));
  });
  return list;
}

// ---------------------------------------------------------------- learn (from home)

P makeButtonPicker(Id remote, PickFn onPick) {
  auto list = std::make_unique<ListScreen>("pick-button", "Botón destino", [remote, onPick](UiContext& c) {
    std::vector<ListItem> it;
    if (const Remote* r = c.library.findRemote(remote)) {
      for (const Button& b : r->buttons) {
        const Id bid = b.id;
        it.push_back({b.name, b.signal ? shortSignal(*b.signal) : std::string("vacío"),
                      [remote, bid, onPick](UiContext& cc) { return onPick(cc, remote, bid, 0); }});
      }
    }
    it.push_back({"+ Nuevo botón", "", [remote, onPick](UiContext&) {
                    return Nav::push(withPresets(std::make_unique<TextInputScreen>(
                        "Nuevo botón", "", limits::kMaxNameBytes,
                        [remote, onPick](UiContext& c2, const std::string& name) {
                          auto r = c2.library.addButton(remote, name);
                          if (!finishChange(c2, r.ok() ? Status::success() : r.status(), "Botón creado")) return Nav::none();
                          Nav next = onPick(c2, remote, r.value(), 1);  // the text editor is still on the stack
                          if (next.kind == Nav::Kind::Push) next.kind = Nav::Kind::Replace;
                          return next;
                        }), buttonPresets()));
                  }});
    return it;
  });
  list->titleFn([remote](UiContext& c) { return "Destino: " + remoteName(c, remote); });
  return list;
}

P makeRemotePicker(const std::string& title, PickFn onPick) {
  auto list = std::make_unique<ListScreen>("pick-remote", title, [onPick](UiContext& c) {
    std::vector<ListItem> it;
    for (const Remote& r : c.library.data().remotes) {
      const Category* cat = c.library.findCategory(r.categoryId);
      const Id rid = r.id;
      it.push_back({r.name, cat ? cat->name : "", [rid, onPick](UiContext&) { return Nav::push(makeButtonPicker(rid, onPick)); }});
    }
    return it;
  });
  list->emptyText("Cree un control antes");
  return list;
}

P makeLearnTargetPicker() {
  return makeRemotePicker("Aprender: control", [](UiContext&, Id remote, Id button, int) {
    return Nav::push(std::make_unique<LearnScreen>(remote, button, 3));
  });
}

// ---------------------------------------------------------------- tools

P makeCaptureDetail(const CaptureRecord& rec) {
  const IrSignal sig = rec.signal;
  auto info = std::make_unique<InfoScreen>("capture-detail", "Captura", [sig](UiContext&) { return signalLines(sig); });
  info->okAction("OK:reproducir RAW", [sig](UiContext&) {
    return Nav::push(std::make_unique<ConfirmScreen>(
        "¿Reproducir RAW?", std::vector<std::string>{"Se emitirá la captura", "con portadora asumida", "de 38 kHz."},
        [sig](UiContext& c) {
          sendSignal(c, rawReplay(sig));
          return Nav::pop();
        }));
  });
  return info;
}

// Last `maxLines` lines of a stored text file, newest last.
std::vector<std::string> storedLogLines(UiContext& c, const std::string& key, size_t maxLines) {
  std::vector<std::string> lines;
  Result<std::string> raw = c.store.read(key);
  if (raw.ok()) {
    size_t start = 0;
    const std::string& t = raw.value();
    while (start < t.size()) {
      size_t end = t.find(char(10), start);
      if (end == std::string::npos) end = t.size();
      if (end > start) lines.push_back(t.substr(start, end - start));
      start = end + 1;
    }
  }
  if (lines.size() > maxLines) lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(maxLines));
  std::reverse(lines.begin(), lines.end());  // newest first, like the event log
  if (lines.empty()) lines.push_back("(sin registro)");
  return lines;
}

P makeEventLogScreen() {
  return std::make_unique<InfoScreen>("event-log", "Registro de eventos", [](UiContext& c) {
    std::vector<std::string> l;
    const auto& entries = c.log.entries();
    const size_t n = entries.size() < 40 ? entries.size() : 40;
    for (size_t i = entries.size(); i > entries.size() - n; --i) {  // newest first
      const LogEntry& e = entries[i - 1];
      l.push_back(numStr(e.timeMs / 1000) + "s " + e.tag + ": " + e.message);
    }
    if (l.empty()) l.push_back("(sin eventos)");
    return l;
  });
}

P makeBootHistoryScreen() {
  return std::make_unique<InfoScreen>("boot-history", "Historial de arranques", [](UiContext& c) {
    std::vector<std::string> l = c.info.bootLines;
    if (l.empty()) l.push_back("(sin datos)");
    return l;
  });
}

P makeExplorationRun() {
  class RunScreen : public Screen {
   public:
    const char* id() const override { return "explore-run"; }
    void onEnter(UiContext& c) override {
      Status st = c.explore.start();
      if (!st.ok()) c.toast(spanishError(st));
    }
    void onExit(UiContext& c) override {
      c.explore.stop();
      if (c.explore.sentCount() > 0) c.explore.saveLog(c.store, "explore.log");  // best effort
      c.explore.reset();
    }
    Nav onKey(UiContext& c, const KeyEvent& e) override {
      if (e.action == KeyAction::Repeat) return Nav::none();
      if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::home();  // onExit stops the run
      if (e.key == Key::Left || e.key == Key::Ok) {
        if (c.explore.state() == ExplorationState::Running) {
          c.explore.stop();
          c.toast("Detenido");
          return Nav::none();
        }
        if (e.action == KeyAction::Press) return Nav::pop();
      }
      return Nav::none();
    }
    void render(UiContext& c, Painter& p) override {
      static const char* names[] = {"Inactiva", "Pendiente de confirmar", "Lista", "En curso", "Detenida", "Completa", "FALLÓ"};
      p.title("Exploración");
      p.text(0, 1, std::string("Estado: ") + names[static_cast<int>(c.explore.state())]);
      p.text(0, 2, "Enviados: " + numStr(c.explore.sentCount()) + "/" + numStr(c.explore.totalCount()));
      if (!c.explore.entries().empty()) {
        const auto& e = c.explore.entries().back();
        p.text(0, 3, "Último: " + hexStr(e.address) + "/" + hexStr(e.command));
      }
      if (c.explore.state() == ExplorationState::Failed) p.text(0, 4, spanishError(c.explore.lastStatus()));
      p.text(0, 5, "El emisor no sabe si");
      p.text(0, 6, "el equipo respondió.");
      p.footer(c.explore.state() == ExplorationState::Running ? "Izq/OK:detener" : "Izq:salir");
    }
  };
  return std::make_unique<RunScreen>();
}

void syncExploreForm(FormScreen& f, size_t changed) {
  const ProtocolVariant* v = syncProtocolFields(f, changed);
  if (!v) return;
  for (size_t i : {2u, 3u}) limitField(f.field(i), maskForBits(v->addressBits));  // address from / to
  for (size_t i : {4u, 5u}) limitField(f.field(i), maskForBits(v->commandBits));  // command from / to
}

ExplorationPlan planFromForm(const FormScreen& f) {
  ExplorationPlan p;
  const ProtocolInfo* info = protocolAt(f.field(0).choice);
  p.protocol = info ? info->id : Protocol::Unknown;
  p.bits = info ? info->variants[f.field(1).choice].bits : 0;
  p.addressFirst = f.field(2).number;
  p.addressLast = f.field(3).number;
  p.commandFirst = f.field(4).number;
  p.commandLast = f.field(5).number;
  p.maxTransmissions = f.field(6).number;
  p.gapMs = f.field(7).number;
  return p;
}

P makeExplorationForm() {
  auto form = std::make_unique<FormScreen>("explore-form", "Exploración");
  form->addChoice("Protocolo", protocolNames());
  form->addChoice("Bits", {"32"});
  form->addNumber("Dir desde", 0, 0, 0xFF, 16);
  form->addNumber("Dir hasta", 0, 0, 0xFF, 16);
  form->addNumber("Cmd desde", 0, 0, 0xFF, 16);
  form->addNumber("Cmd hasta", 15, 0, 0xFF, 16);
  form->addNumber("Máx. envíos", 16, 1, limits::kExplorationHardMaxTransmissions, 10);
  form->addNumber("Pausa (ms)", 500, limits::kExplorationMinGapMs, limits::kExplorationMaxGapMs, 10);
  FormScreen* self = form.get();
  form->addInfo([self]() {
    const uint32_t n = explorationCandidateCount(planFromForm(*self));
    return n ? numStr(n) + " códigos a probar" : std::string("Plan inválido");
  });
  form->addAction("Revisar e iniciar", [](UiContext& c, FormScreen& f) {
    const ExplorationPlan plan = planFromForm(f);
    Status st = c.explore.prepare(plan);
    if (!st.ok()) {
      c.toast(spanishError(st));
      return Nav::none();
    }
    if (c.explore.state() == ExplorationState::AwaitingConfirmation) {
      return Nav::push(std::make_unique<ConfirmScreen>(
          "Exploración larga",
          std::vector<std::string>{numStr(c.explore.totalCount()) + " códigos, pausa " + numStr(plan.gapMs) + " ms",
                                   "Solo con equipos propios.", "El emisor no sabe si el", "equipo respondió.", "¿Iniciar?"},
          [](UiContext& c2) {
            c2.explore.confirm();
            return Nav::replace(makeExplorationRun());
          }));
    }
    return Nav::push(makeExplorationRun());
  });
  form->addAction("Ver registro previo", [](UiContext&, FormScreen&) {
    return Nav::push(std::make_unique<InfoScreen>("explore-log", "Registro exploración",
                                                  [](UiContext& c) { return storedLogLines(c, "explore.log", 60); }));
  });
  form->onChange(syncExploreForm);
  syncExploreForm(*form, 0);
  return form;
}

P makeToolsScreen() {
  return std::make_unique<ListScreen>("tools", "Herramientas IR", [](UiContext&) {
    std::vector<ListItem> it;
    it.push_back({"Últimas capturas", "", [](UiContext&) {
                    auto list = std::make_unique<ListScreen>("captures", "Últimas capturas", [](UiContext& c) {
                      std::vector<ListItem> items;
                      size_t n = 0;
                      for (const CaptureRecord& rec : c.captures) {
                        const CaptureRecord copy = rec;
                        items.push_back({"#" + numStr(++n) + " " + shortSignal(rec.signal), recognitionEs(rec.signal.recognition),
                                         [copy](UiContext&) { return Nav::push(makeCaptureDetail(copy)); }});
                      }
                      return items;
                    });
                    list->emptyText("Sin capturas aún");
                    return Nav::push(std::move(list));
                  }});
    it.push_back({"Exploración", "", [](UiContext&) { return Nav::push(makeExplorationForm()); }});
    it.push_back({"Exportar todo a SD", "", [](UiContext& c) {
                    runExport(c, [&](IFileSink& sink, ExportSummary& s) { return exportAll(sink, c.library, s); });
                    return Nav::none();
                  }});
    return it;
  });
}

// ---------------------------------------------------------------- settings

class ButtonTestScreen : public Screen {
 public:
  const char* id() const override { return "button-test"; }
  Nav onKey(UiContext&, const KeyEvent& e) override {
    if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::pop();
    const char* a = e.action == KeyAction::Press ? "pulso" : e.action == KeyAction::LongPress ? "largo" : "repite";
    last_ = std::string(keyEs(e.key)) + " " + a;
    return Nav::none();
  }
  void render(UiContext& c, Painter& p) override {
    p.title("Prueba de botones");
    for (unsigned i = 0; i < kKeyCount; ++i) {
      const Key k = static_cast<Key>(i);
      p.item(1 + static_cast<int>(i), keyEs(k), c.rawButtons.isPressed(k) ? "[X]" : "[ ]", false);
    }
    p.text(0, 6, "Ult: " + (last_.empty() ? std::string("-") : last_));
    p.footer("Izq largo: salir");
  }

 private:
  std::string last_;
};

// Everything about the device itself, in one place (it used to be split between the tools and the settings).
P makeDiagnosticsMenu() {
  return std::make_unique<ListScreen>("diagnostics-menu", "Diagnóstico", [](UiContext&) {
    std::vector<ListItem> it;
    it.push_back({"Estado", "", [](UiContext&) { return Nav::push(makeDiagnosticsScreen()); }});
    it.push_back({"Info del equipo", "", [](UiContext&) {
                    return Nav::push(std::make_unique<InfoScreen>("device-info", "Info del equipo", [](UiContext& cx) {
                      std::vector<std::string> l;
                      l.push_back("Firmware: " + cx.info.firmware);
                      l.push_back("Pantalla: " + cx.info.displayController);
                      l.push_back("IR: " + cx.info.irLibrary);
                      l.push_back("Almacén: " + cx.info.storage);
                      for (const std::string& pin : cx.info.pinLines) l.push_back(pin);
                      return l;
                    }));
                  }});
    it.push_back({"Prueba de botones", "", [](UiContext&) { return Nav::push(std::make_unique<ButtonTestScreen>()); }});
    it.push_back({"Registro de eventos", "", [](UiContext&) { return Nav::push(makeEventLogScreen()); }});
    it.push_back({"Historial arranques", "", [](UiContext&) { return Nav::push(makeBootHistoryScreen()); }});
    return it;
  });
}

// Changes one preference and saves it; the row shows the new value at once.
ListItem settingRow(std::string label, std::string value, std::function<void(Settings&)> change) {
  return {std::move(label), std::move(value), [change](UiContext& c) {
            change(c.settings);
            if (!c.persistSettings().ok()) c.toast("¡No se pudo guardar!", 3000);
            return Nav::none();
          }};
}

P makeSettingsScreen() {
  return std::make_unique<ListScreen>("settings", "Configuración", [](UiContext& c) {
    std::vector<ListItem> it;
    // The language name is always shown in its own language, so it can be found whatever the current one is.
    it.push_back(settingRow("Idioma", langName(c.settings.language), [](Settings& s) { s.language = nextLang(s.language); }));
    it.push_back(settingRow("Invertir video", c.settings.invertDisplay ? "SÍ" : "NO",
                            [](Settings& s) { s.invertDisplay = !s.invertDisplay; }));
    it.push_back(settingRow("Espera captura", numStr(c.settings.captureTimeoutSec) + " s", [](Settings& s) {
      static const uint8_t steps[] = {5, 10, 15, 30};
      for (size_t i = 0; i < 4; ++i) {
        if (steps[i] == s.captureTimeoutSec) {
          s.captureTimeoutSec = steps[(i + 1) % 4];
          break;
        }
      }
    }));
    it.push_back({"Almacenamiento", "", [](UiContext&) {
                    auto info = std::make_unique<InfoScreen>("storage", "Almacenamiento", [](UiContext& cx) {
                      size_t buttons = 0, withSignal = 0;
                      for (const Remote& r : cx.library.data().remotes) {
                        buttons += r.buttons.size();
                        for (const Button& b : r.buttons) withSignal += b.signal ? 1 : 0;
                      }
                      std::vector<std::string> l;
                      l.push_back("Categorías: " + numStr(cx.library.data().categories.size()) + "/" + numStr(limits::kMaxCategories));
                      l.push_back("Controles: " + numStr(cx.library.data().remotes.size()) + "/" + numStr(limits::kMaxRemotes));
                      l.push_back("Botones: " + numStr(buttons));
                      l.push_back("Con señal: " + numStr(withSignal));
                      l.push_back("RAW: " + numStr(cx.library.totalRawEntries()) + "/" + numStr(limits::kMaxTotalRawEntries));
                      l.push_back(cx.info.storageUsage ? cx.info.storageUsage() : cx.info.storage);
                      l.push_back(std::string("Sin guardar: ") + (cx.library.dirty() ? "SÍ" : "no"));
                      if (cx.info.storageDegraded && cx.info.storageDegraded()) l.insert(l.begin(), "¡FLASH NO MONTADA!");
                      return l;
                    });
                    info->okAction("OK:borrar/formatear", [](UiContext& cx0) {
                      if (cx0.info.storageDegraded && cx0.info.storageDegraded() && cx0.info.storageFormat) {
                        return Nav::push(std::make_unique<ConfirmScreen>(
                            "¿FORMATEAR FLASH?", std::vector<std::string>{"La flash no se pudo leer.", "Formatear borra todo", "lo guardado en ella."},
                            [](UiContext& c3) {
                              Status st = c3.info.storageFormat();
                              c3.toast(st.ok() ? std::string("Flash formateada") : spanishError(st), 3000);
                              return Nav::pop(2);
                            }));
                      }
                      return Nav::push(std::make_unique<ConfirmScreen>(
                          "¿BORRAR TODO?", std::vector<std::string>{"Se eliminan todos los", "controles y señales.", "No se puede deshacer."},
                          [](UiContext& cx) {
                            auto empty = RemoteLibrary::fromData(LibraryData{});
                            cx.library = std::move(empty.value());
                            finishChange(cx, Status::success(), "Todo borrado");
                            return Nav::pop(2);
                          }));
                    });
                    return Nav::push(std::move(info));
                  }});
    it.push_back({"Diagnóstico", "", [](UiContext&) { return Nav::push(makeDiagnosticsMenu()); }});
    return it;
  });
}

// ---------------------------------------------------------------- import from the SD card (Flipper-IRDB)

// `base`, or `base 2`, `base 3`... when `taken` already holds it (always within the name length limit).
std::string uniqueName(const std::vector<std::string>& taken, const std::string& base) {
  auto isTaken = [&](const std::string& n) {
    for (const std::string& t : taken) {
      if (namesEqual(t, n)) return true;
    }
    return false;
  };
  if (!isTaken(base)) return base;
  for (int i = 2; i < 100; ++i) {
    const std::string suffix = " " + std::to_string(i);
    std::string head = base;
    if (head.size() + suffix.size() > limits::kMaxNameBytes) head = utf8Truncate(head, limits::kMaxNameBytes - suffix.size());
    const std::string candidate = trimName(head) + suffix;
    if (!isTaken(candidate)) return candidate;
  }
  return base;
}

// Creates a saved control from a view; names are made unique. Nothing is saved to flash here.
Result<Id> importViewQuiet(UiContext& c, const RemoteView& view, Id cat) {
  std::vector<std::string> names;
  std::vector<std::pair<std::string, IrSignal>> buttons;
  for (const ViewButton& b : view.buttons) {
    if (!b.signal) continue;
    const std::string n = uniqueName(names, b.name);
    names.push_back(n);
    buttons.emplace_back(n, *b.signal);
  }
  std::vector<std::string> remoteNames;
  for (const Remote& r : c.library.data().remotes) remoteNames.push_back(r.name);
  const std::string name = uniqueName(remoteNames, view.name);
  return c.library.importRemote(name, cat, buttons);
}

// Imports a control read from the card (a RemoteView), saves and opens it on top of home.
Nav importViewNav(UiContext& c, const RemoteView& view, Id cat, bool removeCategoryOnFailure) {
  auto r = importViewQuiet(c, view, cat);
  if (!r.ok() && removeCategoryOnFailure) c.library.removeCategory(cat);
  if (!finishChange(c, r.ok() ? Status::success() : r.status(), "Control importado")) return Nav::none();
  return Nav::homeThen(makeRemoteScreen(r.value()));
}

P makeImportViewPicker(std::shared_ptr<RemoteView> view) {
  return std::make_unique<ListScreen>("import-category", "Guardar en...", [view](UiContext& c) {
    std::vector<ListItem> it;
    for (const Category& cat : c.library.data().categories) {
      const Id cid = cat.id;
      it.push_back({cat.name, "", [view, cid](UiContext& c2) { return importViewNav(c2, *view, cid, false); }});
    }
    it.push_back({"+ Nueva categoría", "", [view](UiContext&) {
                    return Nav::push(withPresets(std::make_unique<TextInputScreen>(
                        "Nueva categoría", "", limits::kMaxNameBytes, [view](UiContext& c2, const std::string& catName) {
                          auto cat = c2.library.addCategory(catName);
                          if (!cat.ok()) {
                            c2.toast(spanishError(cat.status()));
                            return Nav::none();
                          }
                          return importViewNav(c2, *view, cat.value(), true);
                        }), categoryPresets()));
                  }});
    return it;
  });
}

// ---------------------------------------------------------------- home

P buildHome() {
  // Six entries, one page. Anything done to one control (create, import, loop, crazy, export) is in Controles, on the
  // Right-key menu of the row it applies to, so each action has a single place.
  auto home = std::make_unique<IconGridScreen>("home", "IRis", [](UiContext&) {
    std::vector<GridItem> it;
    it.push_back({"Controles", icons::kRemote.data(), [](UiContext&) { return Nav::push(makeLibraryScreen()); }});
    it.push_back({"Aprender señal", icons::kLearn.data(), [](UiContext&) { return Nav::push(makeLearnTargetPicker()); }});
    it.push_back({"Escuchar", icons::kListen.data(), [](UiContext&) { return Nav::push(makeListenMode()); }});
    it.push_back({"Be Gone", icons::kBeGone.data(), [](UiContext&) { return Nav::push(makeBeGoneMode()); }});
    it.push_back({"Herramientas IR", icons::kTools.data(), [](UiContext&) { return Nav::push(makeToolsScreen()); }});
    it.push_back({"Configuración", icons::kSettings.data(), [](UiContext&) { return Nav::push(makeSettingsScreen()); }});
    return it;
  });
  // Without a reading: "Bat n/d". With one: an estimated percentage ("~57%") and the bars; the exact voltage is on the
  // diagnostics page.
  // The title and the gauge share one smoothed estimate, so the number does not jump with every sample.
  auto battery = std::make_shared<BatteryEstimator>();
  home->titleFn([battery](UiContext& c) {
    const auto measured = c.battery.readMillivolts();
    battery->update(c.clock.nowMs(), measured ? std::optional<uint16_t>(batteryCellMillivolts(*measured)) : std::nullopt);
    const std::string dirty = c.library.dirty() ? " *" : "";
    if (!battery->valid()) return utf8Justify("IRis", "Bat n/d" + dirty, Painter::kCols);
    return utf8Justify("IRis", std::to_string(battery->percent()) + "%" + dirty, Painter::kCols - 3);
  });
  home->gaugeFn([battery](UiContext&) { return battery->valid() ? battery->segments() : -1; });
  return home;
}

}  // namespace

namespace kit {

std::string numStr(unsigned long long v) { return ::iris::numStr(v); }
std::string shortSignal(const IrSignal& s) { return ::iris::shortSignal(s); }
bool finishChange(UiContext& c, const Status& st, const std::string& okMsg) { return ::iris::finishChange(c, st, okMsg); }
void sendSignal(UiContext& c, const IrSignal& s) { ::iris::sendSignal(c, s); }
IrSignal rawReplay(const IrSignal& captured) { return ::iris::rawReplay(captured); }
std::unique_ptr<Screen> makeRemotePicker(const std::string& title, PickFn onPick) { return ::iris::makeRemotePicker(title, std::move(onPick)); }
std::unique_ptr<Screen> makeCaptureDetail(const CaptureRecord& rec) { return ::iris::makeCaptureDetail(rec); }
Result<Id> importViewQuiet(UiContext& c, const RemoteView& view, Id category) { return ::iris::importViewQuiet(c, view, category); }
std::unique_ptr<Screen> makeImportViewCategoryPicker(std::shared_ptr<RemoteView> view) { return ::iris::makeImportViewPicker(std::move(view)); }

}  // namespace kit

std::unique_ptr<Screen> makeHomeScreen() { return buildHome(); }

}  // namespace iris
