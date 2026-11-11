// More flows: duplicating controls, saving RAW captures, log viewers and the persistence guards.
// Real UI + real services + fake hardware: proves the software flows, not the device.
#include <doctest.h>

#include "UiRig.h"
#include "app/SignalBuilder.h"
#include "domain/Limits.h"
#include "ui_helpers.h"

using namespace iris;
using namespace iris::testkit;
using namespace uihelp;

TEST_CASE("scenario ui: duplicate a control from its menu") {
  UiRig r;
  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  open(r, "Controles");
  open(r, "TV");
  r.press(Key::Right);
  CHECK(open(r, "Duplicar"));
  CHECK(r.shows("Sala 2"));  // the editor starts with a suggested name
  kbFinish(r);
  REQUIRE(r.library.data().remotes.size() == 2);
  CHECK(r.library.data().remotes[1].name == "Sala 2");
  CHECK(r.library.data().remotes[1].buttons[0].signal.has_value());
  CHECK(r.topId() == std::string("category"));
  CHECK(r.shows("Sala 2"));
  CHECK_FALSE(r.library.dirty());
}

TEST_CASE("scenario ui: duplicating with a taken name stays in the editor with a message") {
  UiRig r;
  seed(r);
  open(r, "Controles");
  open(r, "TV");
  r.press(Key::Right);
  open(r, "Duplicar");
  kbErase(r, 2);  // delete "2" and the space
  kbFinish(r);    // "Sala" already exists
  CHECK(r.shows("Ya existe"));
  CHECK(r.library.data().remotes.size() == 1);
  CHECK(r.topId() == std::string("text-input"));
}

TEST_CASE("scenario ui: a capture can be saved into an existing button") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Escuchar");
  r.rx.push(necCapture(0x04, 0x08));
  r.advance(100);
  r.press(Key::Ok);  // stop listening: the list of captures
  r.press(Key::Ok);  // menu of capture #1
  CHECK(open(r, "Guardar en botón"));
  CHECK(r.shows("Guardar en: control"));
  CHECK(open(r, "Sala"));
  CHECK(open(r, "Power"));
  const Button* b = r.library.findButton(ids.remote, ids.button);
  REQUIRE(b->signal.has_value());
  CHECK(b->signal->recognition == Recognition::Recognized);
  CHECK(b->signal->originalCapture.has_value());
  CHECK(r.topId() == std::string("listen"));  // back on the list of captures
  CHECK_FALSE(r.library.dirty());
}

TEST_CASE("scenario ui: a capture can be saved into a brand new button") {
  UiRig r;
  Ids ids = seed(r);
  open(r, "Escuchar");
  ReceivedCapture c;
  c.raw.durationsUs = syntheticUnknownPattern();
  r.rx.push(c);
  r.advance(100);
  r.press(Key::Ok);  // stop listening
  r.press(Key::Ok);  // menu of capture #1
  CHECK(open(r, "Guardar en botón"));
  CHECK(open(r, "Sala"));
  CHECK(open(r, "+ Nuevo botón"));
  typeText(r, "Extra");
  REQUIRE(r.library.findRemote(ids.remote)->buttons.size() == 2);
  const Button& nb = r.library.findRemote(ids.remote)->buttons[1];
  CHECK(nb.name == "Extra");
  REQUIRE(nb.signal.has_value());
  CHECK(nb.signal->recognition == Recognition::Unknown);
  CHECK(r.topId() == std::string("listen"));
}

TEST_CASE("scenario ui: saving a capture into a remote whose raw budget is full is refused and the capture is kept") {
  UiRig r;
  Ids ids = seed(r);
  const size_t perRemote = limits::kMaxRawEntriesPerRemote / limits::kMaxRawEntries;
  IrSignal big = buildRawSignal(std::vector<uint16_t>(limits::kMaxRawEntries, 500)).value();
  for (size_t i = 0; i < perRemote; ++i) {
    Id b = r.library.addButton(ids.remote, "Big" + std::to_string(i)).value();
    REQUIRE(r.library.setButtonSignal(ids.remote, b, big).ok());
  }
  r.library.markClean();
  open(r, "Escuchar");
  ReceivedCapture longCap;
  longCap.raw.durationsUs.assign(1000, 500);  // stored twice (frame + original) -> over the remote budget
  r.rx.push(longCap);
  r.advance(100);
  r.press(Key::Ok);  // stop listening
  REQUIRE(r.shows("Capturas: 1"));
  r.press(Key::Ok);  // menu of capture #1
  CHECK(open(r, "Guardar en botón"));
  open(r, "Sala");
  open(r, "Power");
  CHECK(r.shows("Límite"));
  r.press(Key::Left);
  r.press(Key::Left);
  r.press(Key::Left);
  CHECK(r.topId() == std::string("listen"));
  CHECK(r.shows("Capturas: 1"));  // nothing was lost
  CHECK_FALSE(r.library.findButton(ids.remote, ids.button)->signal.has_value());
}

TEST_CASE("scenario ui: the exploration log of the previous run can be read on the device") {
  UiRig r;
  open(r, "Herramientas IR");
  open(r, "Exploración");
  CHECK(open(r, "Ver registro previo"));
  CHECK(r.shows("(sin registro)"));
  r.press(Key::Left);
  open(r, "Revisar e iniciar");
  r.advance(12000);
  r.press(Key::Left);  // leaving saves the log
  open(r, "Exploración");
  open(r, "Ver registro previo");
  CHECK(r.shows("#15 "));
  CHECK(r.shows("s 0/F"));      // newest first: the last code of the default plan is command 0xF
  for (int i = 0; i < 12; ++i) r.press(Key::Down);
  CHECK(r.shows("#0 "));
  CHECK(r.shows("s 0/0"));      // the oldest line, address 0 command 0
}

TEST_CASE("scenario ui: event log and boot history screens") {
  UiRig r;
  r.info.bootLines = {"#3 BROWNOUT", "#2 encendido"};
  open(r, "Configuración");
  open(r, "Diagnóstico");
  open(r, "Registro de eventos");
  CHECK(r.shows("(sin eventos)"));
  r.press(Key::Left);
  open(r, "Historial arranques");
  CHECK(r.shows("#3 BROWNOUT"));
  CHECK(r.shows("#2 encendido"));
  r.press(Key::Left);

  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  r.longPress(Key::Left);
  open(r, "Controles");
  open(r, "TV");
  open(r, "Sala");
  r.press(Key::Ok);  // send -> logged
  r.longPress(Key::Left);
  open(r, "Configuración");
  open(r, "Diagnóstico");
  open(r, "Registro de eventos");
  CHECK(r.shows("tx: started"));
  r.press(Key::Left);
  r.info.bootLines.clear();
  open(r, "Historial arranques");
  CHECK(r.shows("(sin datos)"));
}

TEST_CASE("scenario ui: with saving disabled changes stay in memory and nothing is written") {
  UiRig r;
  r.ctx.persistEnabled = false;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "TV");
  CHECK(r.shows("Guardado desactivado"));
  CHECK(r.library.data().categories.size() == 1);
  CHECK(r.library.dirty());
  CHECK(r.flash.list().empty());
  r.longPress(Key::Left);
  CHECK(r.shows("*"));  // the unsaved marker stays visible
}

TEST_CASE("scenario ui: the memory guard refuses a save that could run out of RAM") {
  UiRig r;
  Ids ids = seed(r);
  r.library.setButtonSignal(ids.remote, ids.button, buildDecodedSignal(Protocol::Nec, 0, 8, 32).value());
  size_t freeHeap = 1000;
  r.info.freeHeapBytes = [&]() { return freeHeap; };
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "Audio");
  CHECK(r.shows("Memoria insuficiente"));
  CHECK(r.flash.list().empty());
  CHECK(r.library.dirty());
  CHECK(r.log.contains("not enough free memory"));

  freeHeap = 200 * 1024;  // memory came back: the next change saves everything
  open(r, "+ Nueva categoría");
  typeText(r, "Video");
  CHECK_FALSE(r.library.dirty());
  CHECK(r.flash.exists("idx.irs"));
}
