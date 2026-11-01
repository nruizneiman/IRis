// User flows through the real UI, real services and fake hardware, driven by simulated button
// presses (they go through the real debouncer). "scenario" in the test names selects them as
// integration tests. This proves the software flows, not the physical device.
#include <doctest.h>

#include "Fixtures.h"
#include "UiRig.h"
#include "ui_helpers.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"

using namespace iris;
using namespace iris::testkit;

using namespace uihelp;

TEST_CASE("scenario ui: home screen, wrap-around navigation and long-press back") {
  UiRig r;
  CHECK(r.topId() == std::string("home"));
  // Icon grid: one 16x16 icon per entry, a frame on the selected one, only its name as text, battery in the title bar.
  CHECK(r.display.shownBitmaps() == 5);
  CHECK(r.display.shownShapes() == 2);  // title bar + selection frame
  CHECK(isSelected(r.screen(), "Controles"));
  CHECK_FALSE(r.shows("Aprender señal"));
  CHECK(r.shows("Bat n/d"));  // no measurement circuit: never an invented percentage
  r.press(Key::Right);
  CHECK(isSelected(r.screen(), "Aprender señal"));
  r.press(Key::Left);
  CHECK(isSelected(r.screen(), "Controles"));
  r.press(Key::Left);         // wraps to the last of the five
  CHECK(isSelected(r.screen(), "Configuración"));
  r.press(Key::Right);
  r.press(Key::Up);           // a whole row back: 0 -> 2 (wrapping over five entries)
  CHECK(isSelected(r.screen(), "Crear control"));
  r.press(Key::Down);
  CHECK(isSelected(r.screen(), "Controles"));
  r.press(Key::Left);
  r.press(Key::Right);        // nothing below home: it stays on the home screen
  CHECK(r.topId() == std::string("home"));
  CHECK(open(r, "Herramientas IR"));
  CHECK(r.ui.depth() == 2);
  r.longPress(Key::Left);
  CHECK(r.ui.depth() == 1);
}

TEST_CASE("scenario ui: battery voltage is shown when a circuit reports it") {
  UiRig r;
  r.battery.set(3870);
  r.advance(300);
  CHECK(r.shows("3.87 V"));
}

TEST_CASE("scenario ui: create category, remote and button with the text picker") {
  UiRig r;
  CHECK(open(r, "Controles"));
  CHECK(open(r, "+ Nueva categoría"));
  typeText(r, "TV");
  CHECK(r.library.data().categories.size() == 1);
  CHECK(r.library.data().categories[0].name == "TV");
  CHECK(r.shows("TV"));
  CHECK(r.flash.exists("idx.irs"));  // persisted immediately

  CHECK(open(r, "TV"));
  CHECK(open(r, "+ Nuevo control"));
  typeText(r, "Sala");
  REQUIRE(r.library.data().remotes.size() == 1);
  CHECK(r.library.data().remotes[0].name == "Sala");
  CHECK(r.topId() == std::string("remote"));  // new remote is opened

  CHECK(open(r, "+ Nuevo botón"));
  typeText(r, "Vol+");
  REQUIRE(r.library.data().remotes[0].buttons.size() == 1);
  CHECK(r.library.data().remotes[0].buttons[0].name == "Vol+");
  CHECK(r.topId() == std::string("button-menu"));  // goes straight to the button actions
}

TEST_CASE("scenario ui: accented names are supported end to end") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "Niño");
  REQUIRE(r.library.data().categories.size() == 1);
  CHECK(r.library.data().categories[0].name == "Niño");
  CHECK(r.shows("Niño"));
}

TEST_CASE("scenario ui: keyboard backspace, cancel and length limit") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "ABC", false);
  kbErase(r, 1);  // delete C
  kbFinish(r);
  CHECK(r.library.data().categories.at(0).name == "AB");

  open(r, "+ Nueva categoría");
  r.longPress(Key::Left);  // Left long press cancels without saving
  CHECK(r.topId() == std::string("library"));
  CHECK(r.library.data().categories.size() == 1);

  open(r, "+ Nueva categoría");
  typeText(r, std::string(limits::kMaxNameBytes, 'a'), false);
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  REQUIRE(in != nullptr);
  kbMoveTo(r, in, 0, 0);
  r.press(Key::Ok);  // one more key: refused with a message
  CHECK(r.shows("muy largo"));
}

TEST_CASE("scenario ui: duplicate and empty names are rejected with a Spanish message") {
  UiRig r;
  r.library.addCategory("TV");
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "TV");
  CHECK(r.shows("Ya existe"));
  CHECK(r.library.data().categories.size() == 1);
  CHECK(r.topId() == std::string("text-input"));  // stays so the user can fix it
  r.longPress(Key::Left);
  open(r, "+ Nueva categoría");
  kbOpen(r);
  kbFinish(r);  // empty
  CHECK(r.shows("inválido"));
}

TEST_CASE("scenario ui: learn a signal into a button and save it") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  CHECK(r.screen().find("Power") != std::string::npos);
  CHECK(r.shows("vacío"));
  r.press(Key::Ok);  // empty button -> action menu
  CHECK(r.topId() == std::string("button-menu"));
  CHECK(open(r, "Aprender señal"));
  CHECK(r.topId() == std::string("learn"));
  CHECK(r.shows("Esperando"));
  CHECK(r.rx.running());

  r.rx.push(necCapture(0x04, 0x08));
  r.advance(100);
  CHECK(r.shows("Reconocida"));
  CHECK(r.shows("NEC 32 bits"));
  CHECK(r.shows("Dir 0x4 Cmd 0x8"));
  CHECK_FALSE(r.rx.running());

  CHECK(open(r, "Guardar"));
  const Button* b = r.library.findButton(ids.remote, ids.button);
  REQUIRE(b->signal.has_value());
  CHECK(b->signal->recognition == Recognition::Recognized);
  CHECK(b->signal->originalCapture.has_value());  // RAW kept
  CHECK(r.topId() == std::string("remote"));
  CHECK(r.shows("NEC"));
  CHECK_FALSE(r.library.dirty());
  CHECK(r.flash.exists("idx.irs"));
}

TEST_CASE("scenario ui: learn flow from the home screen creates the target button") {
  UiRig r;
  seed(r);
  CHECK(open(r, "Aprender señal"));
  CHECK(open(r, "Sala"));
  CHECK(open(r, "+ Nuevo botón"));
  typeText(r, "Mute");
  CHECK(r.topId() == std::string("learn"));
  r.rx.push(necCapture());
  r.advance(100);
  CHECK(open(r, "Guardar"));
  CHECK(r.topId() == std::string("home"));  // back where the flow started
  REQUIRE(r.library.data().remotes[0].buttons.size() == 2);
  CHECK(r.library.data().remotes[0].buttons[1].name == "Mute");
  CHECK(r.library.data().remotes[0].buttons[1].signal.has_value());
}

TEST_CASE("scenario ui: learn with no controls explains what to do") {
  UiRig r;
  open(r, "Aprender señal");
  CHECK(r.shows("Cree un control"));
}

TEST_CASE("scenario ui: learn timeout, retry, empty capture and cancel") {
  UiRig r;
  seed(r);
  r.settings.captureTimeoutSec = 5;
  open(r, "Aprender señal");
  open(r, "Sala");
  open(r, "Power");
  CHECK(r.topId() == std::string("learn"));
  r.advance(5200);
  CHECK(r.shows("No llegó"));
  CHECK_FALSE(r.rx.running());

  r.press(Key::Ok);  // retry
  CHECK(r.shows("Esperando"));
  r.rx.push(ReceivedCapture{});  // empty capture
  r.advance(100);
  CHECK(r.shows("Captura inválida"));
  CHECK(r.library.findButton(r.library.data().remotes[0].id, r.library.data().remotes[0].buttons[0].id)->signal == std::nullopt);

  r.press(Key::Ok);
  CHECK(r.rx.running());
  r.press(Key::Left);  // cancel while waiting
  CHECK_FALSE(r.rx.running());
  CHECK(r.learn.state() == LearnState::Idle);
}

TEST_CASE("scenario ui: leaving the learn screen by any route stops the receiver") {
  UiRig r;
  seed(r);
  open(r, "Aprender señal");
  open(r, "Sala");
  open(r, "Power");
  REQUIRE(r.rx.running());
  r.longPress(Key::Left);  // straight home
  CHECK_FALSE(r.rx.running());
  CHECK(r.topId() == std::string("home"));
}

TEST_CASE("scenario ui: receiver failure is shown and recoverable") {
  UiRig r;
  seed(r);
  r.rx.setFailStart(true);
  open(r, "Aprender señal");
  open(r, "Sala");
  open(r, "Power");
  CHECK(r.shows("Falló el receptor"));
  r.rx.setFailStart(false);
  r.press(Key::Ok);
  CHECK(r.shows("Esperando"));
}

TEST_CASE("scenario ui: unknown signals can be saved as RAW and noisy ones are flagged") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Aprender señal");
  open(r, "Sala");
  open(r, "Power");
  ReceivedCapture c;
  c.raw.durationsUs = syntheticUnknownPattern();
  r.rx.push(c);
  r.advance(100);
  CHECK(r.shows("Desconocida"));
  CHECK(r.shows("RAW: 13 pulsos"));
  CHECK(r.shows("Portadora desconocida"));
  open(r, "Guardar");
  REQUIRE(r.library.findButton(ids.remote, ids.button)->signal.has_value());
  CHECK(r.library.findButton(ids.remote, ids.button)->signal->recognition == Recognition::Unknown);

  CHECK(r.topId() == std::string("home"));
  open(r, "Aprender señal");  // learn again over the same button: replaces its signal
  open(r, "Sala");
  open(r, "Power");
  ReceivedCapture noisy;
  noisy.raw.durationsUs = syntheticNoiseCapture();
  noisy.decoded = necCapture().decoded;
  r.rx.push(noisy);
  r.advance(100);
  CHECK(r.shows("Parcial"));
  CHECK(r.shows("(ruido)"));
}

TEST_CASE("scenario ui: OK on a button sends, a second press while busy is refused") {
  UiRig r;
  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildRawSignal({500, 60000}, 38000).value());
  // A longer frame so it is still on the air when the second (simulated) press arrives.
  r.library.setButtonSignal(ids.remote, ids.button, buildRawSignal({500, 65000, 500, 65000, 500, 65000, 500, 65000}, 38000).value());
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Ok);
  CHECK(r.shows("Enviando"));
  CHECK(r.tx.busy());
  r.press(Key::Ok);
  CHECK(r.shows("Ocupado"));
  r.advance(500);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(r.tx.history()[0].completed);
}

TEST_CASE("scenario ui: transmission keeps running when the user leaves the screen") {
  UiRig r;
  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildRawSignal({500, 65000, 500, 65000, 500, 65000, 500, 65000}, 38000).value());
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Ok);
  r.longPress(Key::Left);  // home, while the signal is still on the air
  CHECK(r.topId() == std::string("home"));
  r.advance(600);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(r.tx.history()[0].completed);
}

TEST_CASE("scenario ui: sending a button without a signal says so, nothing is transmitted") {
  UiRig r;
  seed(r);
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Right);  // button menu
  CHECK(open(r, "Enviar"));
  CHECK(r.shows("Sin señal"));
  const std::string grid = r.screen();
  const size_t lastNl = grid.rfind('\n', grid.size() - 2);
  CHECK(grid.substr(lastNl + 1) == "Sin señal guardada\n");  // the toast replaces the footer, no leftovers
  CHECK(r.tx.history().empty());
  CHECK(r.tx.state() == TxState::Idle);
}

TEST_CASE("scenario ui: delete asks for confirmation and No keeps the data") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Controles");
  open(r, "TV");
  r.press(Key::Right);  // remote menu for "Sala"
  CHECK(open(r, "Eliminar control"));
  CHECK(r.shows("¿Eliminar control?"));
  r.press(Key::Left);   // No
  CHECK(r.library.findRemote(ids.remote) != nullptr);
  CHECK(open(r, "Eliminar control"));
  r.press(Key::Ok);     // Yes
  CHECK(r.library.findRemote(ids.remote) == nullptr);
  CHECK(r.topId() == std::string("category"));
  CHECK_FALSE(r.shows("Sala"));
}

TEST_CASE("scenario ui: a category that still has remotes cannot be deleted") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Controles");
  r.press(Key::Right);
  CHECK(open(r, "Eliminar categoría"));
  r.press(Key::Ok);
  CHECK(r.shows("En uso"));
  CHECK(r.library.findCategory(ids.cat) != nullptr);
}

TEST_CASE("scenario ui: rename a button, delete its signal, delete the button") {
  UiRig r;
  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Right);
  CHECK(open(r, "Renombrar"));
  kbErase(r, 5);  // erase "Power"
  typeText(r, "Enc");
  CHECK(r.library.findButton(ids.remote, ids.button)->name == "Enc");
  CHECK(r.topId() == std::string("remote"));

  r.press(Key::Right);
  CHECK(open(r, "Borrar señal"));
  r.press(Key::Ok);
  CHECK_FALSE(r.library.findButton(ids.remote, ids.button)->signal.has_value());

  r.press(Key::Right);
  CHECK(open(r, "Eliminar botón"));
  r.press(Key::Ok);
  CHECK(r.library.findButton(ids.remote, ids.button) == nullptr);
}

TEST_CASE("scenario ui: move a remote to another category") {
  UiRig r;
  Ids ids = seed(r);
  Id other = r.library.addCategory("Audio").value();
  open(r, "Controles");
  open(r, "TV");
  r.press(Key::Right);
  CHECK(open(r, "Cambiar categoría"));
  CHECK(open(r, "Audio"));
  CHECK(r.library.findRemote(ids.remote)->categoryId == other);
}

TEST_CASE("scenario ui: create control from the home screen, with a new category") {
  UiRig r;
  CHECK(open(r, "Crear control"));
  typeText(r, "Equipo");
  CHECK(r.shows("+ Nueva categoría"));
  CHECK(open(r, "+ Nueva categoría"));
  typeText(r, "Audio");
  REQUIRE(r.library.data().remotes.size() == 1);
  CHECK(r.library.data().remotes[0].name == "Equipo");
  CHECK(r.library.data().categories[0].name == "Audio");
  CHECK(r.topId() == std::string("remote"));
  r.press(Key::Left);
  CHECK(r.topId() == std::string("home"));
}

TEST_CASE("scenario ui: create a manual signal with the form and number editor") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Ok);  // empty button -> menu
  CHECK(open(r, "Crear manual"));
  CHECK(r.shows("NEC 32b 0x0/0x0"));  // live preview
  CHECK(open(r, "Dirección"));
  for (int i = 0; i < 4; ++i) r.press(Key::Up);
  r.press(Key::Ok);
  CHECK(open(r, "Comando"));
  for (int i = 0; i < 8; ++i) r.press(Key::Up);
  r.press(Key::Ok);
  CHECK(r.shows("NEC 32b 0x4/0x8"));
  CHECK(open(r, "Probar envío"));
  CHECK(r.shows("Enviando"));
  r.advance(200);
  REQUIRE(r.tx.history().size() == 1);

  CHECK(open(r, "Guardar"));
  const Button* b = r.library.findButton(ids.remote, ids.button);
  REQUIRE(b->signal.has_value());
  CHECK(b->signal->provenance == Provenance::Manual);
  CHECK(b->signal->initial[0].decoded.address == 4);
  CHECK(b->signal->initial[0].decoded.command == 8);
  CHECK(r.topId() == std::string("remote"));
}

TEST_CASE("scenario ui: manual form follows the protocol (bit options, field widths, toggle)") {
  UiRig r;
  seed(r);
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Ok);
  open(r, "Crear manual");
  CHECK_FALSE(r.shows("Toggle"));
  CHECK(select(r, "Protocolo"));
  r.press(Key::Ok);  // NEC -> Samsung
  r.press(Key::Ok);  // -> Sony
  CHECK(r.shows("Sony"));
  CHECK(open(r, "Dirección"));
  r.press(Key::Left);          // cursor to the high digit
  r.press(Key::Up);
  r.press(Key::Up);            // 0x20 > 5-bit maximum 0x1F
  r.press(Key::Ok);
  CHECK(r.shows("Rango"));
  CHECK(r.topId() == std::string("number-input"));  // not accepted
  r.longPress(Key::Left);      // cancel the editor
  CHECK(r.topId() == std::string("manual-signal"));
  CHECK(select(r, "Protocolo"));
  r.press(Key::Ok);            // Sony -> RC5
  CHECK(r.shows("RC5"));
  CHECK(r.shows("Toggle"));    // only protocols with a toggle bit show it
}

TEST_CASE("scenario ui: tools - receiver info, diagnostics and capture history") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Info del receptor");
  CHECK(r.shows("TSOP38238"));
  CHECK(r.shows("detenido"));
  r.press(Key::Left);
  open(r, "Diagnóstico HW");
  CHECK(r.shows("Pantalla: OK"));
  CHECK(r.shows("Batería: n/d"));
  CHECK(r.shows("Receptor: detenido"));
  CHECK(r.shows("Emisor: reposo"));
  r.longPress(Key::Left);

  open(r, "Herramientas IR");
  open(r, "Últimas capturas");
  CHECK(r.shows("Sin capturas"));
  r.press(Key::Left);
  open(r, "Reproducir RAW");
  CHECK(r.shows("Sin capturas"));
  CHECK(r.tx.history().empty());
}

TEST_CASE("scenario ui: raw capture tool records history and replays RAW only after confirmation") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Captura RAW");
  CHECK(r.topId() == std::string("raw-capture"));
  r.rx.push(necCapture());
  r.advance(100);
  CHECK(r.shows("Reconocida"));
  CHECK(open(r, "Reproducir"));
  CHECK(r.shows("Enviando"));
  r.advance(300);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(r.tx.history()[0].job.signal.initial[0].kind == Frame::Kind::Raw);
  CHECK(r.tx.history()[0].job.signal.initial[0].raw.carrierHz == 0);  // unknown, transmitter default applies
  r.longPress(Key::Left);

  open(r, "Herramientas IR");
  open(r, "Últimas capturas");
  CHECK(r.shows("#1 NEC"));
  r.press(Key::Left);
  open(r, "Reproducir RAW");
  CHECK(r.topId() == std::string("capture-detail"));
  r.press(Key::Ok);  // asks first
  CHECK(r.topId() == std::string("confirm"));
  CHECK(r.shows("38 kHz"));
  r.press(Key::Left);
  CHECK(r.tx.history().size() == 1);  // declined: nothing more sent
  r.press(Key::Ok);
  r.press(Key::Ok);
  r.advance(300);
  CHECK(r.tx.history().size() == 2);
}

TEST_CASE("scenario ui: exploration runs a small plan to completion and logs it") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Exploración");
  CHECK(r.shows("16 códigos"));
  CHECK(open(r, "Revisar e iniciar"));
  CHECK(r.topId() == std::string("explore-run"));  // 16 codes: no extra confirmation
  r.advance(12000);
  CHECK(r.shows("Completa"));
  CHECK(r.shows("Enviados: 16/16"));
  CHECK(r.tx.history().size() == 16);
  r.press(Key::Left);
  CHECK(r.flash.exists("explore.log"));
  CHECK(r.explore.state() == ExplorationState::Idle);
}

TEST_CASE("scenario ui: exploration can be stopped and then sends nothing more") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Exploración");
  open(r, "Revisar e iniciar");
  r.advance(1500);
  const uint32_t sent = r.explore.sentCount();
  REQUIRE(sent >= 1);
  r.press(Key::Left);
  CHECK(r.explore.state() == ExplorationState::Stopped);
  CHECK(r.shows("Detenida"));
  r.advance(5000);
  CHECK(r.explore.sentCount() == sent);
  CHECK_FALSE(r.tx.busy());
}

TEST_CASE("scenario ui: long exploration requires confirmation; declining sends nothing") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Exploración");
  CHECK(open(r, "Cmd hasta"));
  r.press(Key::Left);
  r.press(Key::Down);  // high digit 0 -> F : 0xFF
  r.press(Key::Ok);
  CHECK(open(r, "Máx. envíos"));
  r.press(Key::Left);
  r.press(Key::Left);
  r.press(Key::Up);    // 16 -> 116
  r.press(Key::Ok);
  CHECK(r.shows("116 códigos"));
  CHECK(open(r, "Revisar e iniciar"));
  CHECK(r.topId() == std::string("confirm"));
  CHECK(r.shows("Exploración larga"));
  CHECK(r.shows("respondió"));
  r.press(Key::Left);  // decline
  r.advance(3000);
  CHECK(r.tx.history().empty());
  CHECK(r.explore.state() == ExplorationState::AwaitingConfirmation);

  CHECK(open(r, "Revisar e iniciar"));
  r.press(Key::Ok);    // accept
  CHECK(r.topId() == std::string("explore-run"));
  r.advance(1500);
  CHECK(r.explore.state() == ExplorationState::Running);
  r.longPress(Key::Left);  // leaving the screen stops the run
  CHECK(r.explore.state() != ExplorationState::Running);
  const size_t sent = r.tx.history().size();
  r.advance(3000);
  CHECK(r.tx.history().size() == sent);
}

TEST_CASE("scenario ui: exploration form rejects invalid ranges") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Exploración");
  CHECK(open(r, "Cmd desde"));
  r.press(Key::Left);
  r.press(Key::Up);  // from 0x10 > to 0x0F
  r.press(Key::Ok);
  CHECK(r.shows("Plan inválido"));
  CHECK(open(r, "Revisar e iniciar"));
  CHECK(r.shows("inválido"));
  CHECK(r.tx.history().empty());
}

TEST_CASE("scenario ui: settings are applied and persisted") {
  UiRig r;
  open(r, "Configuración");
  CHECK(open(r, "Invertir video"));
  CHECK(r.shows("SÍ"));
  CHECK(r.display.inverted());
  CHECK(open(r, "Espera captura"));
  CHECK(r.shows("30 s"));
  SettingsStore store(r.flash);
  Settings saved = store.load();
  CHECK(saved.invertDisplay);
  CHECK(saved.captureTimeoutSec == 30);
}

TEST_CASE("scenario ui: button test screen shows live raw levels and last event") {
  UiRig r;
  open(r, "Configuración");
  open(r, "Prueba de botones");
  CHECK(r.topId() == std::string("button-test"));
  r.buttons.set(Key::Right, true);
  r.advance(300);
  CHECK(r.shows("[X]"));
  r.buttons.set(Key::Right, false);
  r.advance(100);
  CHECK(r.shows("Derecha pulso"));  // short presses are reported on release
  r.press(Key::Left);  // a normal press is only recorded, it does not leave
  CHECK(r.topId() == std::string("button-test"));
  CHECK(r.shows("Izquierda pulso"));
  r.longPress(Key::Left);
  CHECK(r.topId() == std::string("settings"));
}

TEST_CASE("scenario ui: storage page and wipe-all with confirmation") {
  UiRig r;
  seed(r);
  r.library.setButtonSignal(r.library.data().remotes[0].id, r.library.data().remotes[0].buttons[0].id,
                            buildRawSignal({500, 500, 500, 500}).value());
  open(r, "Configuración");
  open(r, "Almacenamiento");
  CHECK(r.shows("Controles: 1/32"));
  CHECK(r.shows("Con señal: 1"));
  CHECK(r.shows("RAW: 4/"));
  r.press(Key::Ok);
  CHECK(r.topId() == std::string("confirm"));
  CHECK(r.shows("BORRAR TODO"));
  r.press(Key::Left);
  CHECK(r.library.data().remotes.size() == 1);
  r.press(Key::Ok);
  r.press(Key::Ok);
  CHECK(r.library.data().remotes.empty());
  CHECK(r.library.data().categories.empty());
  CHECK(r.topId() == std::string("settings"));
  LibraryRepository repo(r.flash);
  auto loaded = repo.load();
  REQUIRE(loaded.ok());
  CHECK(loaded.value().library.data().remotes.empty());  // the wipe is persisted
}

TEST_CASE("scenario ui: display failure never blocks navigation and recovers") {
  UiRig r;
  r.display.setFailFlush(true);
  r.press(Key::Down);
  CHECK_FALSE(r.ctx.displayHealthy);
  CHECK(r.log.contains("flush failed"));
  r.press(Key::Ok);  // blind navigation: the Down above already moved the selection to Herramientas IR (0 -> 3)
  CHECK(r.topId() == std::string("tools"));  // the UI logic keeps working without a picture
  r.display.setFailFlush(false);
  r.advance(2500);
  CHECK(r.ctx.displayHealthy);
  CHECK(r.shows("Info del receptor"));
  CHECK(r.log.contains("recovered"));
}

TEST_CASE("scenario ui: a failed save is reported, the change stays and the home shows it is unsaved") {
  UiRig r;
  r.flash.failWritesAfter(0);
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "TV");
  CHECK(r.shows("No se pudo guardar"));
  CHECK(r.library.data().categories.size() == 1);
  CHECK(r.library.dirty());
  r.longPress(Key::Left);
  CHECK(r.shows("*"));  // unsaved marker next to the battery text

  r.flash.failWritesAfter(-1);
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "Audio");
  CHECK_FALSE(r.library.dirty());  // the next successful save writes everything
  r.longPress(Key::Left);
  CHECK_FALSE(r.shows(" *"));
}

TEST_CASE("scenario ui: capturing keeps working while another screen is shown") {
  UiRig r;
  seed(r);
  r.learn.start(10000);              // armed from elsewhere (e.g. a future background hook)
  r.rx.push(necCapture());
  open(r, "Herramientas IR");        // the user is on a different screen
  r.advance(200);
  CHECK(r.learn.state() == LearnState::Captured);
  CHECK(r.ctx.captures.size() == 1);  // the capture history is filled regardless of the visible screen
}

TEST_CASE("scenario ui: a flash that could not be mounted is flagged and formatting needs confirmation") {
  UiRig r;
  bool degraded = true;
  int formatted = 0;
  r.info.storageDegraded = [&]() { return degraded; };
  r.info.storageFormat = [&]() {
    ++formatted;
    degraded = false;
    return Status::success();
  };
  r.info.storageUsage = []() { return std::string("RAM (sin flash)"); };
  open(r, "Configuración");
  open(r, "Almacenamiento");
  CHECK(r.shows("FLASH NO MONTADA"));  // the warning is the first line
  r.press(Key::Down);
  r.press(Key::Down);                  // scroll to the usage line
  CHECK(r.shows("RAM (sin flash)"));
  r.press(Key::Ok);
  CHECK(r.shows("FORMATEAR"));
  r.press(Key::Left);  // No
  CHECK(formatted == 0);
  r.press(Key::Ok);
  r.press(Key::Ok);    // Yes
  CHECK(formatted == 1);
  CHECK(r.shows("formateada"));
  CHECK(r.topId() == std::string("settings"));
}

TEST_CASE("scenario ui: adapter counters appear in the diagnostics page") {
  UiRig r;
  r.info.adapterCounters = []() { return std::vector<std::string>{"RX vistos:3 rep:1", "TX ok:2 canc:0 rech:1"}; };
  open(r, "Herramientas IR");
  open(r, "Diagnóstico HW");
  for (int i = 0; i < 4; ++i) r.press(Key::Down);
  CHECK(r.shows("RX vistos:3 rep:1"));
  CHECK(r.shows("TX ok:2 canc:0 rech:1"));
}
