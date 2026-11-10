#include "ui/ModuleScreens.h"

#include <algorithm>
#include <cctype>

#include "app/RemoteView.h"
#include "app/Sequences.h"
#include "domain/Limits.h"
#include "domain/Protocol.h"
#include "ui/ScreenKit.h"

namespace iris {

namespace {

using P = std::unique_ptr<Screen>;
using namespace iris::kit;

constexpr size_t kCardListMax = 300;  // entries shown per folder

struct PauseOption {
  uint32_t ms;
  const char* label;
};
constexpr PauseOption kPauses[] = {{200, "0.2 s"}, {500, "0.5 s"},  {1000, "1 s"},  {2000, "2 s"},
                                   {5000, "5 s"},  {10000, "10 s"}, {30000, "30 s"}, {60000, "60 s"}};
constexpr size_t kPauseCount = sizeof kPauses / sizeof kPauses[0];

size_t pauseIndexFor(uint32_t ms) {
  for (size_t i = 0; i < kPauseCount; ++i) {
    if (kPauses[i].ms == ms) return i;
  }
  return 2;
}

std::string lowered(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

Result<RemoteView> openRef(UiContext& c, const RemoteRef& ref) {
  if (ref.fromCard()) {
    if (!c.files) return Status::fail(Err::Unavailable, "no card");
    return viewCardRemote(*c.files, ref.path);
  }
  return viewSavedRemote(c.library, ref.savedId);
}

std::string cardErrorText(const Status& st) {
  return st.code == Err::Unavailable          ? "Sin tarjeta SD"
         : st.code == Err::OutOfRange         ? "Archivo demasiado grande"
         : st.code == Err::UnsupportedVersion ? "Versión no soportada"
         : st.code == Err::Corrupt            ? "No es un archivo IRDB"
                                              : "No se pudo leer";
}

// ---------------------------------------------------------------- running a sequence

// Shows a run of the SequenceRunner. `start` is called when the screen opens (and again on OK after a run ended), so
// nothing is sent before the user has chosen to open this screen. Leaving the screen stops the run.
class SequenceRunScreen : public Screen {
 public:
  using StartFn = std::function<Status(UiContext&)>;
  SequenceRunScreen(std::string title, StartFn start) : title_(std::move(title)), start_(std::move(start)) {}

  const char* id() const override { return "sequence-run"; }
  void onEnter(UiContext& c) override { begin(c); }
  void onExit(UiContext& c) override {
    c.sequence.stop();
    c.sequence.reset();
  }

  Nav onKey(UiContext& c, const KeyEvent& e) override {
    if (e.action == KeyAction::Repeat) return Nav::none();
    if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::home();  // onExit stops the run
    if (e.action != KeyAction::Press) return Nav::none();
    if (e.key == Key::Left || e.key == Key::Ok) {
      if (c.sequence.running()) {
        c.sequence.stop();
        c.toast("Detenido");
        return Nav::none();
      }
      if (e.key == Key::Left) return Nav::pop();
      begin(c);  // OK after the run ended: again
    }
    return Nav::none();
  }

  void render(UiContext& c, Painter& p) override {
    p.title(title_);
    const char* state = c.sequence.state() == SequenceState::Running   ? "En curso"
                        : c.sequence.state() == SequenceState::Done    ? "Terminado"
                        : c.sequence.state() == SequenceState::Stopped ? "Detenido"
                                                                       : "Listo";
    p.text(0, 1, std::string("Estado: ") + state);
    p.text(0, 2, "Enviados: " + numStr(c.sequence.sent()));
    if (!c.sequence.last().empty()) p.text(0, 3, "Último: " + c.sequence.last());
    const std::string progress = c.sequence.progress();
    if (!progress.empty()) p.text(0, 4, progress);
    if (c.sequence.skipped()) p.text(0, 5, "Omitidos: " + numStr(c.sequence.skipped()));
    p.text(0, 6, "El emisor no sabe si el");
    p.footer(c.sequence.running() ? "Izq/OK:detener" : "OK:repetir Izq:salir");
  }

 private:
  void begin(UiContext& c) {
    c.sequence.reset();
    const Status st = start_(c);
    if (!st.ok()) c.toast(spanishError(st));
  }
  std::string title_;
  StartFn start_;
};

// ---------------------------------------------------------------- card browsing

struct Listing {
  std::vector<FileEntry> entries;
  bool cut = false;
  std::string error;  // empty when the listing worked
};

Listing listCard(UiContext& c, const std::string& dir, bool isRoot) {
  Listing out;
  if (!c.files || !c.files->available()) {
    out.error = "Sin tarjeta SD";
    return out;
  }
  auto r = c.files->list(dir, kCardListMax + 1);
  if (!r.ok()) {
    out.error = isRoot ? "Sin carpeta /irdb" : "No se pudo leer";
    return out;
  }
  for (FileEntry& e : r.value()) {
    if (e.name.empty() || e.name[0] == '.') continue;
    if (e.isDir || endsWithIr(e.name)) out.entries.push_back(std::move(e));
  }
  out.cut = out.entries.size() > kCardListMax;
  if (out.cut) out.entries.resize(kCardListMax);
  std::sort(out.entries.begin(), out.entries.end(), [](const FileEntry& a, const FileEntry& b) {
    if (a.isDir != b.isDir) return a.isDir;  // folders first
    return lowered(a.name) < lowered(b.name);
  });
  return out;
}

}  // namespace

P makeCardBrowser(CardBrowserOptions o) {
  auto opts = std::make_shared<CardBrowserOptions>(std::move(o));
  auto list = std::make_unique<ListScreen>(opts->id, opts->rootTitle, [opts](UiContext& c) {
    std::vector<ListItem> it;
    const Listing l = listCard(c, opts->dir, opts->dir == opts->root);
    if (!l.error.empty()) {
      it.push_back({l.error, "", nullptr});
      return it;
    }
    const std::string dir = opts->dir;
    if (opts->onFolder) {
      it.push_back({opts->folderLabel, "", [opts, dir](UiContext& c2) { return opts->onFolder(c2, dir); }});
    }
    for (const FileEntry& e : l.entries) {
      const std::string name = e.name;
      if (e.isDir) {
        it.push_back({name + "/", "", [opts, dir, name](UiContext&) {
                        CardBrowserOptions next = *opts;
                        next.dir = dir + "/" + name;
                        return Nav::push(makeCardBrowser(std::move(next)));
                      }});
      } else {
        it.push_back({name.substr(0, name.size() - 3), "", [opts, dir, name](UiContext& c2) { return opts->onFile(c2, dir, name); }});
      }
    }
    if (l.cut) it.push_back({"(lista incompleta)", "", nullptr});
    return it;
  });
  list->titleFn([opts](UiContext&) {
    if (opts->dir == opts->root) return opts->rootTitle;
    const size_t slash = opts->dir.rfind('/');
    return slash == std::string::npos ? opts->dir : opts->dir.substr(slash + 1);
  });
  list->emptyText("(carpeta vacía)");
  return list;
}

std::vector<ListItem> cardLibraryItems(UiContext& c) {
  std::vector<ListItem> it;
  if (!c.settings.cardInLibrary || !c.files || !c.files->available()) return it;
  const Listing l = listCard(c, "/irdb", true);
  if (!l.error.empty()) return it;
  for (const FileEntry& e : l.entries) {
    if (!e.isDir) continue;
    const std::string name = e.name;
    it.push_back({name + "/", "SD", [name](UiContext&) {
                    CardBrowserOptions o;
                    o.id = "card-library";
                    o.rootTitle = name;
                    o.root = "/irdb/" + name;
                    o.dir = o.root;
                    o.onFile = [](UiContext&, const std::string& dir, const std::string& file) {
                      return Nav::push(makeCardRemoteScreen(dir + "/" + file));
                    };
                    return Nav::push(makeCardBrowser(std::move(o)));
                  }});
  }
  return it;
}

namespace {

P makeCardRemoteMenu(const std::string& path, std::shared_ptr<RemoteView> view);

}  // namespace

P makeCardRemoteScreen(const std::string& path) {
  struct State {
    bool loaded = false;
    Status status;
    std::shared_ptr<RemoteView> view;
  };
  auto st = std::make_shared<State>();
  auto list = std::make_unique<ListScreen>("card-remote", "Control", [st, path](UiContext& c) {
    std::vector<ListItem> it;
    if (!st->loaded) {
      st->loaded = true;
      auto v = openRef(c, RemoteRef{0, path});
      if (v.ok()) st->view = std::make_shared<RemoteView>(std::move(v.value()));
      else st->status = v.status();
    }
    if (!st->view) {
      it.push_back({cardErrorText(st->status), "", nullptr});
      return it;
    }
    const std::shared_ptr<RemoteView> view = st->view;
    for (size_t i = 0; i < view->buttons.size(); ++i) {
      const ViewButton& b = view->buttons[i];
      it.push_back({b.name, b.signal ? shortSignal(*b.signal) : std::string("vacío"), [view, i](UiContext& cc) {
                      if (!view->buttons[i].signal) return Nav::none();
                      sendSignal(cc, *view->buttons[i].signal);
                      return Nav::none();
                    }});
    }
    return it;
  });
  list->titleFn([st, path](UiContext&) {
    if (st->view) return st->view->name;
    const size_t slash = path.rfind('/');
    return remoteNameFromFile(slash == std::string::npos ? path : path.substr(slash + 1));
  });
  list->footer("OK:enviar Der:menú");
  list->onContext([st, path](UiContext&, size_t) {
    if (!st->view) return Nav::none();
    return Nav::push(makeCardRemoteMenu(path, st->view));
  });
  return list;
}

namespace {

P makeCardRemoteMenu(const std::string& path, std::shared_ptr<RemoteView> view) {
  return std::make_unique<ListScreen>("card-remote-menu", "Control SD", [path, view](UiContext& c) {
    (void)c;
    std::vector<ListItem> it;
    it.push_back({"Importar", "", [view](UiContext&) { return Nav::push(makeImportViewCategoryPicker(view)); }});
    it.push_back({"Modo loop", "", [path](UiContext&) { return Nav::push(makeLoopSetup(RemoteRef{0, path})); }});
    it.push_back({"Modo loco", "", [path](UiContext&) { return Nav::push(makeCrazySetup(RemoteRef{0, path})); }});
    return it;
  });
}

// ---------------------------------------------------------------- picking a control (saved or card)

P makeSavedRemoteList(const std::string& title, std::function<Nav(UiContext&, RemoteRef)> onPick) {
  auto list = std::make_unique<ListScreen>("pick-saved-remote", title, [onPick](UiContext& c) {
    std::vector<ListItem> it;
    for (const Remote& r : c.library.data().remotes) {
      const Category* cat = c.library.findCategory(r.categoryId);
      const Id rid = r.id;
      it.push_back({r.name, cat ? cat->name : "", [rid, onPick](UiContext& c2) { return onPick(c2, RemoteRef{rid, ""}); }});
    }
    return it;
  });
  list->emptyText("Cree un control antes");
  return list;
}

}  // namespace

P makeRemoteSourcePicker(const std::string& title, std::function<Nav(UiContext&, RemoteRef)> onPick) {
  return std::make_unique<ListScreen>("pick-source", title, [title, onPick](UiContext& c) {
    std::vector<ListItem> it;
    it.push_back({"Guardados", numStr(c.library.data().remotes.size()), [title, onPick](UiContext&) {
                    return Nav::push(makeSavedRemoteList(title, onPick));
                  }});
    if (c.files && c.files->available()) {
      it.push_back({"Tarjeta SD", "", [title, onPick](UiContext&) {
                      CardBrowserOptions o;
                      o.id = "pick-card-remote";
                      o.rootTitle = title;
                      o.onFile = [onPick](UiContext& c2, const std::string& dir, const std::string& file) {
                        return onPick(c2, RemoteRef{0, dir + "/" + file});
                      };
                      return Nav::push(makeCardBrowser(std::move(o)));
                    }});
    }
    return it;
  });
}

// ---------------------------------------------------------------- loop

namespace {

struct LoopState {
  RemoteView view;
  std::vector<size_t> order;
  size_t pause = 2;
};

}  // namespace

P makeLoopSetup(RemoteRef remote) {
  struct Holder {
    bool loaded = false;
    Status status;
    std::shared_ptr<LoopState> state;
  };
  auto h = std::make_shared<Holder>();
  auto list = std::make_unique<ListScreen>("loop-setup", "Modo loop", [h, remote](UiContext& c) {
    std::vector<ListItem> it;
    if (!h->loaded) {
      h->loaded = true;
      auto v = openRef(c, remote);
      if (v.ok()) {
        h->state = std::make_shared<LoopState>();
        h->state->view = std::move(v.value());
      } else {
        h->status = v.status();
      }
    }
    if (!h->state) {
      it.push_back({cardErrorText(h->status), "", nullptr});
      return it;
    }
    const std::shared_ptr<LoopState> st = h->state;
    it.push_back({"Iniciar", numStr(st->order.size()) + " bot.", [st](UiContext&) {
                    if (st->order.empty()) {
                      return Nav::push(std::make_unique<ConfirmScreen>("Elija botones", std::vector<std::string>{"Marque al menos uno", "con OK en la lista."},
                                                                       [](UiContext&) { return Nav::pop(); }));
                    }
                    const RemoteView view = st->view;
                    const std::vector<size_t> order = st->order;
                    const uint32_t gap = kPauses[st->pause].ms;
                    return Nav::push(std::make_unique<SequenceRunScreen>("Loop: " + view.name, [view, order, gap](UiContext& c2) {
                      return c2.sequence.start(std::make_unique<LoopSequence>(view, order), gap);
                    }));
                  }});
    it.push_back({"Pausa", kPauses[st->pause].label, [st](UiContext&) {
                    st->pause = (st->pause + 1) % kPauseCount;
                    return Nav::none();
                  }});
    for (size_t i = 0; i < st->view.buttons.size(); ++i) {
      const ViewButton& b = st->view.buttons[i];
      if (!b.signal) continue;
      const auto pos = std::find(st->order.begin(), st->order.end(), i);
      const std::string mark = pos == st->order.end() ? std::string() : "#" + numStr(static_cast<size_t>(pos - st->order.begin()) + 1);
      it.push_back({b.name, mark, [st, i](UiContext&) {
                      auto at = std::find(st->order.begin(), st->order.end(), i);
                      if (at == st->order.end()) st->order.push_back(i);
                      else st->order.erase(at);
                      return Nav::none();
                    }});
    }
    return it;
  });
  list->footer("OK:marcar Izq:atrás");
  list->titleFn([h](UiContext&) { return h->state ? "Loop: " + h->state->view.name : std::string("Modo loop"); });
  return list;
}

// ---------------------------------------------------------------- crazy

P makeCrazySetup(RemoteRef remote) {
  auto form = std::make_unique<FormScreen>("crazy-setup", "Modo loco");
  std::vector<std::string> pauses;
  for (const PauseOption& p : kPauses) pauses.push_back(p.label);
  form->addChoice("Pausa media", pauses, pauseIndexFor(1000));
  form->addInfo([]() { return std::string("Botones al azar,"); });
  form->addInfo([]() { return std::string("a veces repetidos."); });
  form->addAction("Iniciar", [remote](UiContext& c, FormScreen& f) {
    auto view = openRef(c, remote);
    if (!view.ok()) {
      c.toast(cardErrorText(view.status()));
      return Nav::none();
    }
    const uint32_t gap = kPauses[f.field(0).choice].ms;
    const RemoteView v = std::move(view.value());
    const uint32_t seed = c.clock.nowMs() ^ 0xA5A5A5A5u;
    return Nav::push(std::make_unique<SequenceRunScreen>("Loco: " + v.name, [v, gap, seed](UiContext& c2) {
      return c2.sequence.start(std::make_unique<CrazySequence>(v, seed), gap);
    }));
  });
  return form;
}

P makeLoopMode() {
  return makeRemoteSourcePicker("Loop: control", [](UiContext&, RemoteRef r) { return Nav::push(makeLoopSetup(std::move(r))); });
}

P makeCrazyMode() {
  return makeRemoteSourcePicker("Loco: control", [](UiContext&, RemoteRef r) { return Nav::push(makeCrazySetup(std::move(r))); });
}

// ---------------------------------------------------------------- Be Gone

namespace {

struct BeGoneChoice {
  PowerWanted wanted = PowerWanted::Off;
  uint32_t gapMs = 1000;
};

// Ask once more before sending hundreds of codes, then run.
Nav confirmBeGone(const BeGoneChoice& choice, const std::string& what, std::function<std::unique_ptr<IRemoteViewSource>(UiContext&)> source) {
  const std::string action = choice.wanted == PowerWanted::Off ? "Apagar" : "Encender";
  return Nav::push(std::make_unique<ConfirmScreen>(
      "¿Iniciar Be Gone?", std::vector<std::string>{action + ": " + what, "Envía muchos códigos.", "Solo con equipos propios."},
      [choice, source](UiContext&) {
        return Nav::replace(std::make_unique<SequenceRunScreen>("Be Gone", [choice, source](UiContext& c2) {
          return c2.sequence.start(std::make_unique<BeGoneSequence>(source(c2), choice.wanted), choice.gapMs);
        }));
      }));
}

P makeBeGoneSourcePicker(BeGoneChoice choice) {
  return std::make_unique<ListScreen>("begone-source", "Be Gone: categoría", [choice](UiContext& c) {
    std::vector<ListItem> it;
    for (const Category& cat : c.library.data().categories) {
      size_t n = 0;
      for (const Remote& r : c.library.data().remotes) n += r.categoryId == cat.id ? 1 : 0;
      const Id cid = cat.id;
      const std::string name = cat.name;
      it.push_back({name, numStr(n), [choice, cid, name](UiContext&) {
                      return confirmBeGone(choice, name, [cid](UiContext& c2) -> std::unique_ptr<IRemoteViewSource> {
                        return std::make_unique<SavedCategorySource>(c2.library, cid);
                      });
                    }});
    }
    if (c.files && c.files->available()) {
      it.push_back({"Tarjeta SD", "", [choice](UiContext&) {
                      CardBrowserOptions o;
                      o.id = "begone-card";
                      o.rootTitle = "Be Gone: carpeta";
                      o.onFile = [](UiContext&, const std::string&, const std::string&) { return Nav::none(); };
                      o.onFolder = [choice](UiContext&, const std::string& dir) {
                        const size_t slash = dir.rfind('/');
                        const std::string what = slash == std::string::npos ? dir : dir.substr(slash + 1);
                        return confirmBeGone(choice, what, [dir](UiContext& c2) -> std::unique_ptr<IRemoteViewSource> {
                          return std::make_unique<CardFolderSource>(*c2.files, dir);
                        });
                      };
                      return Nav::push(makeCardBrowser(std::move(o)));
                    }});
    }
    return it;
  });
}

}  // namespace

P makeBeGoneMode() {
  auto form = std::make_unique<FormScreen>("begone", "Be Gone");
  form->addChoice("Acción", {"Apagar", "Encender"}, 0);
  std::vector<std::string> pauses;
  for (const PauseOption& p : kPauses) pauses.push_back(p.label);
  form->addChoice("Pausa", pauses, pauseIndexFor(1000));
  form->addAction("Elegir categoría", [](UiContext&, FormScreen& f) {
    BeGoneChoice choice;
    choice.wanted = f.field(0).choice == 0 ? PowerWanted::Off : PowerWanted::On;
    choice.gapMs = kPauses[f.field(1).choice].ms;
    return Nav::push(makeBeGoneSourcePicker(choice));
  });
  return form;
}

// ---------------------------------------------------------------- active listening

namespace {

constexpr size_t kListenMaxCaptures = 40;
constexpr size_t kListenMaxRawEntries = 16000;

struct ListenData {
  std::vector<CaptureRecord> items;
  size_t rawEntries = 0;
};

size_t rawCount(const IrSignal& s) {
  size_t n = 0;
  for (const Frame& f : s.initial) n += f.raw.durationsUs.size();
  if (s.originalCapture) n += s.originalCapture->durationsUs.size();
  return n;
}

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%lX", static_cast<unsigned long>(v));
  return b;
}

std::string captureSummary(const IrSignal& s) {
  if (!s.initial.empty() && s.initial[0].kind == Frame::Kind::Decoded) {
    const DecodedFrame& d = s.initial[0].decoded;
    const ProtocolInfo* p = findProtocol(d.protocol);
    return std::string(p ? p->displayName : "?") + " " + hex(d.address) + "/" + hex(d.command);
  }
  return "RAW " + numStr(s.initial.empty() ? 0 : s.initial[0].raw.durationsUs.size());
}

// An exact replay for a recognized signal, the RAW one otherwise.
void replayCapture(UiContext& c, const IrSignal& s) {
  const bool exact = s.recognition == Recognition::Recognized && !s.initial.empty() && s.initial[0].kind == Frame::Kind::Decoded;
  sendSignal(c, exact ? s : rawReplay(s));
}

P makeCaptureActions(std::shared_ptr<ListenData> data, size_t index) {
  auto list = std::make_unique<ListScreen>("listen-capture", "Captura", [data, index](UiContext&) {
    std::vector<ListItem> it;
    if (index >= data->items.size()) return it;
    it.push_back({"Reproducir", "", [data, index](UiContext& c) {
                    if (index < data->items.size()) replayCapture(c, data->items[index].signal);
                    return Nav::none();
                  }});
    it.push_back({"Guardar en botón", "", [data, index](UiContext&) {
                    return Nav::push(makeRemotePicker("Guardar en: control", [data, index](UiContext& c2, Id remote, Id button, int extra) {
                      if (index >= data->items.size()) {
                        c2.toast("Sin captura");
                        return Nav::none();
                      }
                      Status st = c2.library.setButtonSignal(remote, button, data->items[index].signal);
                      if (!finishChange(c2, st, "Señal guardada")) return Nav::none();
                      return Nav::pop(3 + extra);  // button picker, control picker, this menu
                    }));
                  }});
    it.push_back({"Detalle", "", [data, index](UiContext&) {
                    if (index >= data->items.size()) return Nav::none();
                    return Nav::push(makeCaptureDetail(data->items[index]));
                  }});
    it.push_back({"Eliminar", "", [data, index](UiContext&) {
                    if (index < data->items.size()) {
                      data->rawEntries -= std::min(data->rawEntries, rawCount(data->items[index].signal));
                      data->items.erase(data->items.begin() + static_cast<std::ptrdiff_t>(index));
                    }
                    return Nav::pop();
                  }});
    return it;
  });
  list->titleFn([data, index](UiContext&) {
    return index < data->items.size() ? "#" + numStr(index + 1) + " " + captureSummary(data->items[index].signal) : std::string("Captura");
  });
  return list;
}

class ListenScreen : public Screen {
 public:
  const char* id() const override { return "listen"; }
  void onEnter(UiContext& c) override { listen(c); }
  void onExit(UiContext& c) override { c.learn.cancel(); }
  void onResume(UiContext&) override { clampSelection(); }

  void onTick(UiContext& c) override {
    if (!listening_) return;
    switch (c.learn.state()) {
      case LearnState::Captured: {
        const IrSignal& got = *c.learn.result();
        const size_t n = rawCount(got);
        if (data_->items.size() >= kListenMaxCaptures || data_->rawEntries + n > kListenMaxRawEntries) {
          stop(c);
          c.toast("Memoria llena");
          return;
        }
        data_->items.push_back(CaptureRecord{c.clock.nowMs(), got});
        data_->rawEntries += n;
        c.learn.take();
        arm(c);
        break;
      }
      case LearnState::Rejected:
      case LearnState::TimedOut:
      case LearnState::Idle:
        arm(c);
        break;
      case LearnState::Failed:
        stop(c);
        c.toast(spanishError(c.learn.lastStatus()));
        break;
      case LearnState::Armed:
        break;
    }
  }

  Nav onKey(UiContext& c, const KeyEvent& e) override {
    if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::home();
    if (listening_) {
      if (e.key == Key::Ok && e.action == KeyAction::Press) stop(c);
      if (e.key == Key::Left && e.action == KeyAction::Press) return Nav::pop();
      return Nav::none();
    }
    const size_t rows = data_->items.size() + 1;  // the captures and "Escuchar de nuevo"
    if ((e.key == Key::Up || e.key == Key::Down) && e.action != KeyAction::LongPress) {
      sel_ = e.key == Key::Down ? (sel_ + 1) % rows : (sel_ + rows - 1) % rows;
      return Nav::none();
    }
    if (e.action != KeyAction::Press) return Nav::none();
    if (e.key == Key::Left) return Nav::pop();
    if (e.key == Key::Ok || e.key == Key::Right) {
      if (sel_ == data_->items.size()) {
        if (e.key == Key::Ok) listen(c);
        return Nav::none();
      }
      return Nav::push(makeCaptureActions(data_, sel_));
    }
    return Nav::none();
  }

  void render(UiContext& c, Painter& p) override {
    if (listening_) {
      p.title("Escuchando");
      p.text(0, 1, "Capturas: " + numStr(data_->items.size()));
      if (!data_->items.empty()) p.text(0, 2, captureSummary(data_->items.back().signal));
      p.text(0, 4, "Pulse botones de un");
      p.text(0, 5, "control hacia el");
      p.text(0, 6, "receptor.");
      p.footer("OK:detener Izq:salir");
      (void)c;
      return;
    }
    p.title("Capturas: " + numStr(data_->items.size()));
    const size_t rows = data_->items.size() + 1;
    clampSelection();
    if (sel_ < top_) top_ = sel_;
    if (sel_ >= top_ + Painter::kContentRows) top_ = sel_ + 1 - Painter::kContentRows;
    for (size_t r = 0; r < static_cast<size_t>(Painter::kContentRows) && top_ + r < rows; ++r) {
      const size_t i = top_ + r;
      const std::string label = i < data_->items.size() ? "#" + numStr(i + 1) + " " + captureSummary(data_->items[i].signal)
                                                        : std::string("Escuchar de nuevo");
      p.item(Painter::kFirstContentRow + static_cast<int>(r), label, "", i == sel_);
    }
    p.scrollbar(rows, top_, Painter::kContentRows);
    p.footer("OK:ver Der:menú");
  }

 private:
  void arm(UiContext& c) {
    const Status st = c.learn.start(LearnSession::kMaxTimeoutMs);
    if (!st.ok()) {
      stop(c);
      c.toast(spanishError(st));
    }
  }
  void listen(UiContext& c) {
    listening_ = true;
    sel_ = top_ = 0;
    arm(c);
  }
  void stop(UiContext& c) {
    listening_ = false;
    c.learn.cancel();
    sel_ = 0;
  }
  void clampSelection() {
    const size_t rows = data_->items.size() + 1;
    if (sel_ >= rows) sel_ = rows - 1;
  }

  std::shared_ptr<ListenData> data_ = std::make_shared<ListenData>();
  bool listening_ = false;
  size_t sel_ = 0, top_ = 0;
};

}  // namespace

P makeListenMode() { return std::make_unique<ListenScreen>(); }

// ---------------------------------------------------------------- importing a whole folder

namespace {

// Works through the `.ir` files of one folder, one per tick, so the screen stays alive and Left can stop it.
class ImportFolderScreen : public Screen {
 public:
  ImportFolderScreen(std::string dir, Id category, bool createdCategory)
      : dir_(std::move(dir)), category_(category), created_(createdCategory) {}

  const char* id() const override { return "import-folder"; }

  void onEnter(UiContext& c) override {
    const Listing l = listCard(c, dir_, false);
    if (!l.error.empty()) {
      message_ = l.error;
      finish(c);
      return;
    }
    for (const FileEntry& e : l.entries) {
      if (!e.isDir) files_.push_back(e.name);
    }
    if (files_.empty()) message_ = "(carpeta vacía)";
    running_ = !files_.empty();
    if (!running_) finish(c);
  }
  void onExit(UiContext& c) override {
    if (running_) finish(c);
  }

  void onTick(UiContext& c) override {
    if (!running_) return;
    if (next_ >= files_.size()) {
      finish(c);
      return;
    }
    const std::string path = dir_ + "/" + files_[next_++];
    auto view = c.files ? viewCardRemote(*c.files, path) : Result<RemoteView>(Status::fail(Err::Unavailable, "no card"));
    if (!view.ok()) {
      ++failed_;
      return;
    }
    const bool hasSignal = std::any_of(view.value().buttons.begin(), view.value().buttons.end(),
                                       [](const ViewButton& b) { return b.signal.has_value(); });
    if (!hasSignal) {
      ++failed_;
      return;
    }
    auto r = importViewQuiet(c, view.value(), category_);
    if (r.ok()) {
      ++imported_;
    } else if (r.status().code == Err::LimitReached) {
      message_ = "Sin lugar";  // the library is full: the rest cannot be imported
      finish(c);
    } else {
      ++failed_;
    }
  }

  Nav onKey(UiContext& c, const KeyEvent& e) override {
    if (e.action == KeyAction::Repeat) return Nav::none();
    if (e.key == Key::Left && e.action == KeyAction::LongPress) return Nav::home();
    if (e.action != KeyAction::Press) return Nav::none();
    if (e.key == Key::Left || e.key == Key::Ok) {
      if (running_) {
        message_ = "Detenido";
        finish(c);
        return Nav::none();
      }
      return Nav::pop();
    }
    return Nav::none();
  }

  void render(UiContext&, Painter& p) override {
    p.title("Importar carpeta");
    p.text(0, 1, "Archivos: " + numStr(std::min(next_, files_.size())) + "/" + numStr(files_.size()));
    p.text(0, 2, "Importados: " + numStr(imported_));
    if (failed_) p.text(0, 3, "Omitidos: " + numStr(failed_));
    if (!message_.empty()) p.text(0, 5, message_);
    p.footer(running_ ? "Izq:detener" : "Izq:salir");
  }

 private:
  void finish(UiContext& c) {
    if (!running_ && finished_) return;
    running_ = false;
    finished_ = true;
    if (imported_ == 0 && created_) {
      c.library.removeCategory(category_);  // nothing arrived: do not leave an empty category behind
      return;
    }
    if (imported_ > 0) {
      const Status st = c.persist();
      if (!st.ok()) c.toast("¡No se pudo guardar!", 3000);
      else if (message_.empty()) message_ = "Listo";
    }
  }

  std::string dir_;
  Id category_;
  bool created_;
  std::vector<std::string> files_;
  size_t next_ = 0, imported_ = 0, failed_ = 0;
  bool running_ = false, finished_ = false;
  std::string message_;
};

}  // namespace

P makeImportFolderPicker(const std::string& dir) {
  const size_t slash = dir.rfind('/');
  const std::string folder = slash == std::string::npos ? dir : dir.substr(slash + 1);
  return std::make_unique<ListScreen>("import-folder-target", "Guardar en...", [dir, folder](UiContext& c) {
    std::vector<ListItem> it;
    it.push_back({"+ " + folder, "", [dir, folder](UiContext& c2) {
                    auto cat = c2.library.addCategory(folder);
                    if (!cat.ok()) {
                      c2.toast(spanishError(cat.status()));
                      return Nav::none();
                    }
                    return Nav::replace(std::make_unique<ImportFolderScreen>(dir, cat.value(), true));
                  }});
    for (const Category& cat : c.library.data().categories) {
      const Id cid = cat.id;
      it.push_back({cat.name, "", [dir, cid](UiContext&) { return Nav::replace(std::make_unique<ImportFolderScreen>(dir, cid, false)); }});
    }
    return it;
  });
}

}  // namespace iris
