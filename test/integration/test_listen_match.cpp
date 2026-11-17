// Listening: find the controls (saved and on the card) that hold what was heard, and keep the captures in a control.
#include <doctest.h>

#include "app/SignalBuilder.h"
#include "app/SignalMatch.h"
#include "ui_helpers.h"

using namespace uihelp;

namespace {

std::string irFile(std::initializer_list<std::pair<const char*, int>> buttons) {
  std::string t = "Filetype: IR signals file\nVersion: 1\n";
  for (const auto& b : buttons) {
    char line[160];
    std::snprintf(line, sizeof line, "#\nname: %s\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: %02X 00 00 00\n", b.first,
                  b.second);
    t += line;
  }
  return t;
}

IrSignal nec(uint32_t addr, uint32_t cmd) { return buildDecodedSignal(Protocol::Nec, addr, cmd, 32).value(); }

IrSignal rc5(uint32_t addr, uint32_t cmd, bool toggle) {
  IrSignal s = buildDecodedSignal(Protocol::Rc5, addr, cmd, 13).value();
  s.initial[0].decoded.toggle = toggle;
  return s;
}

IrSignal rawOnly() {
  IrSignal s;
  s.initial.push_back(Frame::makeRaw(RawFrame{{9000, 4500, 560, 560}, 0}));
  return s;
}

void hear(UiRig& r, uint32_t addr, uint32_t cmd) {
  r.rx.push(necCapture(addr, cmd));
  r.advance(500);
}

}  // namespace

TEST_CASE("sameCode: compares protocol, address, command and size, and ignores the toggle bit") {
  CHECK(sameCode(nec(4, 8), nec(4, 8)));
  CHECK_FALSE(sameCode(nec(4, 8), nec(4, 9)));
  CHECK_FALSE(sameCode(nec(4, 8), nec(5, 8)));
  CHECK(sameCode(rc5(5, 12, false), rc5(5, 12, true)));
  CHECK_FALSE(sameCode(nec(4, 8), rc5(4, 8, false)));
  CHECK_FALSE(sameCode(rawOnly(), rawOnly()));
  CHECK_FALSE(isMatchable(rawOnly()));
}

TEST_CASE("matcher: saved controls first, the card after, controls with more of the heard codes first") {
  FakeFileSource card;
  card.add("/irdb/TVs/LG/Both.ir", irFile({{"Power", 8}, {"Vol_up", 9}}));
  card.add("/irdb/TVs/LG/One.ir", irFile({{"Power", 8}}));
  card.add("/irdb/TVs/LG/None.ir", irFile({{"Power", 0x40}}));
  card.add("/irdb/TVs/LG/Broken.ir", "not a remote");

  RemoteLibrary lib;
  const Id cat = lib.addCategory("TV").value();
  const Id rid = lib.addRemote("Sala", cat).value();
  const Id pw = lib.addButton(rid, "Power").value();
  REQUIRE(lib.setButtonSignal(rid, pw, nec(4, 8)).ok());
  lib.addRemote("Vacio", cat);

  // heard: two codes, one of them twice, one that cannot be looked up
  RemoteMatcher m(lib, &card, {nec(4, 8), nec(4, 9), nec(4, 8), rawOnly()});
  CHECK(m.heard().size() == 2);
  REQUIRE(m.hits().size() == 1);  // the saved control, found right away
  CHECK(m.hits()[0].remote == "Sala");
  CHECK_FALSE(m.hits()[0].fromCard);

  while (m.step()) {}
  CHECK(m.done());
  REQUIRE(m.hits().size() == 3);
  m.sortByScore();
  CHECK(m.hits()[0].remote == "Both");
  CHECK(m.hits()[0].fromCard);
  CHECK(m.hits()[0].heardMatched == 2);
  CHECK(m.hits()[0].buttons.size() == 2);
  CHECK(m.hits()[0].path == "/irdb/TVs/LG/Both.ir");
  CHECK(m.hits()[1].heardMatched == 1);
  CHECK(m.hits()[2].heardMatched == 1);
}

TEST_CASE("matcher: nothing to look up for a RAW capture, and no card means only the saved controls") {
  RemoteLibrary lib;
  const Id cat = lib.addCategory("TV").value();
  const Id rid = lib.addRemote("Sala", cat).value();
  lib.setButtonSignal(rid, lib.addButton(rid, "Power").value(), nec(4, 8));

  RemoteMatcher raw(lib, nullptr, {rawOnly()});
  CHECK(raw.done());
  CHECK(raw.heard().empty());
  CHECK(raw.hits().empty());

  RemoteMatcher noCard(lib, nullptr, {nec(4, 8)});
  CHECK(noCard.done());
  CHECK(noCard.hits().size() == 1);
  CHECK_FALSE(noCard.step());
}

TEST_CASE("scenario ui: a capture finds the saved control and the card control that have its code") {
  UiRig r;
  const Ids ids = seed(r);
  REQUIRE(r.library.setButtonSignal(ids.remote, ids.button, nec(4, 8)).ok());
  r.card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}, {"Vol_up", 9}}));
  r.card.add("/irdb/TVs/Other/Other.ir", irFile({{"Mute", 0x40}}));

  REQUIRE(open(r, "Escuchar"));
  hear(r, 4, 8);
  r.press(Key::Ok);  // stop: the list of captures
  r.press(Key::Right);
  REQUIRE(open(r, "Buscar controles"));
  CHECK(r.topId() == std::string("match"));
  r.advance(100);
  CHECK(r.shows("Coincidencias: 2"));
  CHECK(r.shows("Sala"));
  CHECK(r.shows("LG TV"));
  CHECK_FALSE(r.shows("Other"));
  CHECK(r.shows("SD"));

  // open the card hit and send its matching button
  REQUIRE(select(r, "LG TV"));
  r.press(Key::Ok);
  CHECK(r.topId() == std::string("match-hit"));
  REQUIRE(open(r, "Power"));
  r.advance(300);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(r.tx.history().at(0).job.signal.initial.at(0).decoded.command == 8);
  CHECK_FALSE(r.shows("Vol_up"));  // only the buttons that matched are offered
}

TEST_CASE("scenario ui: every capture at once ranks the controls by how many of the codes they hold") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/Both.ir", irFile({{"Power", 8}, {"Vol_up", 9}}));
  r.card.add("/irdb/TVs/LG/AAA_One.ir", irFile({{"Power", 8}}));
  REQUIRE(open(r, "Escuchar"));
  hear(r, 4, 8);
  hear(r, 4, 9);
  r.press(Key::Ok);
  REQUIRE(open(r, "Buscar en controles"));
  r.advance(100);
  CHECK(r.shows("2/2 SD"));
  CHECK(r.shows("1/2 SD"));
  CHECK(isSelected(r.screen(), "Both"));  // the control with both codes comes first, although it sorts later by name
}

TEST_CASE("scenario ui: a RAW capture has nothing to look up") {
  UiRig r;
  REQUIRE(open(r, "Escuchar"));
  ReceivedCapture c;
  c.raw.durationsUs = syntheticUnknownPattern();
  r.rx.push(c);
  r.advance(500);
  r.press(Key::Ok);
  r.press(Key::Right);
  REQUIRE(open(r, "Buscar controles"));
  CHECK(r.topId() == std::string("listen-capture"));  // stays on the menu
  CHECK(r.shows("sin código"));
}

TEST_CASE("scenario ui: the captures are kept as buttons of a new control, once per different code") {
  UiRig r;
  seed(r);
  REQUIRE(open(r, "Escuchar"));
  hear(r, 4, 8);
  hear(r, 4, 9);
  hear(r, 4, 8);  // pressed twice
  r.press(Key::Ok);
  REQUIRE(open(r, "Guardar todas"));
  REQUIRE(open(r, "+ Nuevo control"));
  typeText(r, "Mando");
  REQUIRE(open(r, "TV"));
  CHECK(r.topId() == std::string("listen"));
  const Remote* made = nullptr;
  for (const Remote& rem : r.library.data().remotes) {
    if (rem.name == "Mando") made = &rem;
  }
  REQUIRE(made != nullptr);
  REQUIRE(made->buttons.size() == 2);
  CHECK(made->buttons[0].name == "Tecla 1");
  CHECK(made->buttons[1].signal->initial.at(0).decoded.command == 9);
}

TEST_CASE("scenario ui: the captures can be added to a saved control without touching its buttons") {
  UiRig r;
  const Ids ids = seed(r);
  REQUIRE(open(r, "Escuchar"));
  hear(r, 4, 8);
  hear(r, 4, 9);
  r.press(Key::Ok);
  REQUIRE(open(r, "Guardar todas"));
  REQUIRE(open(r, "Sala"));
  CHECK(r.topId() == std::string("listen"));
  const Remote* sala = r.library.findRemote(ids.remote);
  REQUIRE(sala != nullptr);
  REQUIRE(sala->buttons.size() == 3);
  CHECK(sala->buttons[0].name == "Power");
  CHECK_FALSE(sala->buttons[0].signal.has_value());
  CHECK(sala->buttons[1].name == "Tecla 1");
  CHECK(sala->buttons[2].name == "Tecla 2");
}
