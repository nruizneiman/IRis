#include <doctest.h>

#include "app/SignalBuilder.h"
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

uint32_t sentCommand(const UiRig& r, size_t i) { return r.tx.history().at(i).job.signal.initial.at(0).decoded.command; }

}  // namespace

TEST_CASE("sd controls: the card's folders sit next to the saved categories, and a card control can be operated") {
  UiRig r;
  seed(r);
  r.card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}, {"Vol_up", 2}}));
  REQUIRE(open(r, "Controles"));
  CHECK(r.shows("TV"));          // the saved category
  CHECK(r.shows("TVs/"));        // the card's folder, with "SD" on its right
  CHECK(r.shows("SD"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "LG TV") == false);  // files are listed by their file name, not the control name
  REQUIRE(open(r, "LG_TV"));
  CHECK(r.topId() == std::string("card-remote"));
  CHECK(r.shows("LG TV"));       // the title is the control's name
  REQUIRE(select(r, "Power"));
  r.press(Key::Ok);
  r.advance(300);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(sentCommand(r, 0) == 8);
}

TEST_CASE("sd controls: absent without a card; a control lying directly in /irdb is listed too") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}}));
  r.card.add("/irdb/Loose.ir", irFile({{"Power", 9}}));
  REQUIRE(open(r, "Controles"));
  CHECK(r.shows("TVs/"));
  REQUIRE(open(r, "Loose"));
  CHECK(r.topId() == std::string("card-remote"));
  r.longPress(Key::Left);
  r.card.setPresent(false);
  REQUIRE(open(r, "Controles"));
  CHECK_FALSE(r.shows("TVs/"));
  CHECK_FALSE(r.shows("Loose"));
}

TEST_CASE("sd controls: a card control can be imported from its menu, and saved ones still work as before") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}, {"Vol_up", 2}}));
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "LG_TV"));
  r.press(Key::Right);  // menu
  REQUIRE(open(r, "Importar"));
  REQUIRE(open(r, "+ Nueva categoría"));
  typeText(r, "TV");
  REQUIRE(r.library.data().remotes.size() == 1);
  CHECK(r.library.data().remotes[0].name == "LG TV");
  CHECK(r.library.data().remotes[0].buttons.size() == 2);
  CHECK_FALSE(r.library.dirty());
}

TEST_CASE("loop: from the menu of a control, mark buttons, set the pause and run; Left stops it") {
  UiRig r;
  const Ids ids = seed(r);
  const Id b2 = r.library.addButton(ids.remote, "Vol+").value();
  REQUIRE(r.library.setButtonSignal(ids.remote, ids.button, nec(1, 1)).ok());
  REQUIRE(r.library.setButtonSignal(ids.remote, b2, nec(1, 2)).ok());
  r.library.markClean();
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TV"));
  r.press(Key::Right);  // menu of the control "Sala"
  REQUIRE(open(r, "Modo loop"));
  CHECK(r.topId() == std::string("loop-setup"));
  r.press(Key::Ok);  // "Iniciar" with nothing marked
  CHECK(r.shows("Elija botones"));
  r.press(Key::Left);
  REQUIRE(open(r, "Vol+"));   // marked first
  REQUIRE(open(r, "Power"));  // marked second
  CHECK(r.shows("#1"));
  CHECK(r.shows("#2"));
  REQUIRE(select(r, "Iniciar"));
  r.press(Key::Ok);
  CHECK(r.topId() == std::string("sequence-run"));
  r.advance(5000);
  CHECK(r.ctx.sequence.running());
  CHECK(r.ctx.sequence.sent() >= 3);
  CHECK(sentCommand(r, 0) == 2);  // Vol+ first, then Power
  CHECK(sentCommand(r, 1) == 1);
  CHECK(sentCommand(r, 2) == 2);
  r.press(Key::Left);             // stop
  const uint32_t at = r.ctx.sequence.sent();
  r.advance(5000);
  CHECK(r.ctx.sequence.sent() == at);
  r.press(Key::Left);             // leave
  CHECK(r.topId() == std::string("loop-setup"));
}

TEST_CASE("loop: also works on a control read from the card, from its menu") {
  UiRig r;
  const Ids ids = seed(r);
  REQUIRE(r.library.setButtonSignal(ids.remote, ids.button, nec(1, 1)).ok());
  r.library.markClean();
  r.card.add("/irdb/TVs/LG/LG_TV.ir", irFile({{"Power", 8}}));
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "LG_TV"));
  r.press(Key::Right);  // menu of the card control
  REQUIRE(open(r, "Modo loop"));
  CHECK(r.topId() == std::string("loop-setup"));
  CHECK(r.shows("Power"));
}

TEST_CASE("crazy mode: random buttons of the chosen control until stopped") {
  UiRig r;
  const Ids ids = seed(r);
  const Id b2 = r.library.addButton(ids.remote, "Vol+").value();
  const Id b3 = r.library.addButton(ids.remote, "Vol-").value();
  REQUIRE(r.library.setButtonSignal(ids.remote, ids.button, nec(1, 1)).ok());
  REQUIRE(r.library.setButtonSignal(ids.remote, b2, nec(1, 2)).ok());
  REQUIRE(r.library.setButtonSignal(ids.remote, b3, nec(1, 3)).ok());
  r.library.markClean();
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TV"));
  r.press(Key::Right);  // menu of the control "Sala"
  REQUIRE(open(r, "Modo loco"));
  CHECK(r.topId() == std::string("crazy-setup"));
  REQUIRE(select(r, "Iniciar"));
  r.press(Key::Ok);
  CHECK(r.topId() == std::string("sequence-run"));
  r.advance(30000);
  CHECK(r.ctx.sequence.running());
  CHECK(r.ctx.sequence.sent() >= 10);
  bool seen[4] = {false, false, false, false};
  for (const auto& h : r.tx.history()) seen[h.job.signal.initial.at(0).decoded.command] = true;
  CHECK(seen[1]);
  CHECK(seen[2]);
  CHECK(seen[3]);
  r.longPress(Key::Left);  // home: leaving stops it
  CHECK(r.topId() == std::string("home"));
  CHECK_FALSE(r.ctx.sequence.running());
}

TEST_CASE("Be Gone: asks first, then sends one power code per control of the category, once per distinct code") {
  UiRig r;
  const Id cat = r.library.addCategory("TV").value();
  for (auto [name, cmd] : {std::pair<const char*, uint32_t>{"A", 1}, {"B", 2}, {"C", 2}}) {  // B and C share a code
    const Id rem = r.library.addRemote(name, cat).value();
    const Id b = r.library.addButton(rem, "Power").value();
    REQUIRE(r.library.setButtonSignal(rem, b, nec(1, cmd)).ok());
  }
  r.library.markClean();
  REQUIRE(open(r, "Be Gone"));
  REQUIRE(select(r, "Elegir categoría"));
  r.press(Key::Ok);
  REQUIRE(open(r, "TV"));
  CHECK(r.topId() == std::string("confirm"));
  CHECK(r.shows("Solo con equipos"));
  CHECK(r.tx.history().empty());  // nothing is sent before the answer
  r.press(Key::Ok);               // yes
  CHECK(r.topId() == std::string("sequence-run"));
  r.advance(8000);
  CHECK(r.ctx.sequence.state() == SequenceState::Done);
  REQUIRE(r.tx.history().size() == 2);
  CHECK(sentCommand(r, 0) == 1);
  CHECK(sentCommand(r, 1) == 2);
}

TEST_CASE("Be Gone: over a card folder, and answering no sends nothing") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/a.ir", irFile({{"Power", 8}}));
  r.card.add("/irdb/TVs/Sony/b.ir", irFile({{"Power", 9}}));
  REQUIRE(open(r, "Be Gone"));
  REQUIRE(select(r, "Elegir categoría"));
  r.press(Key::Ok);
  REQUIRE(open(r, "Tarjeta SD"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "» Usar esta carpeta"));
  CHECK(r.topId() == std::string("confirm"));
  r.press(Key::Left);  // no
  r.advance(3000);
  CHECK(r.tx.history().empty());
  REQUIRE(open(r, "» Usar esta carpeta"));
  r.press(Key::Ok);
  r.advance(8000);
  CHECK(r.ctx.sequence.state() == SequenceState::Done);
  CHECK(r.tx.history().size() == 2);
}

TEST_CASE("listen: keeps receiving until stopped, then lists the captures to replay or save") {
  UiRig r;
  const Ids ids = seed(r);
  REQUIRE(open(r, "Escuchar"));
  CHECK(r.topId() == std::string("listen"));
  CHECK(r.shows("Escuchando"));
  r.rx.push(necCapture(0x04, 0x08));
  r.advance(500);
  r.rx.push(necCapture(0x04, 0x09));
  r.advance(500);
  r.rx.push(necCapture(0x04, 0x0A));
  r.advance(500);
  CHECK(r.shows("Capturas: 3"));
  r.press(Key::Ok);  // stop
  CHECK(r.shows("#1"));
  CHECK(r.shows("#3"));
  CHECK_FALSE(r.rx.running());
  // replay the second one
  r.press(Key::Down);
  r.press(Key::Right);  // its menu
  REQUIRE(open(r, "Reproducir"));
  r.advance(300);
  REQUIRE(r.tx.history().size() == 1);
  CHECK(sentCommand(r, 0) == 9);
  // save it into the button of the saved control
  REQUIRE(open(r, "Guardar en botón"));
  REQUIRE(open(r, "Sala"));
  REQUIRE(open(r, "Power"));
  CHECK(r.topId() == std::string("listen"));
  const Button* b = r.library.findButton(ids.remote, ids.button);
  REQUIRE(b != nullptr);
  REQUIRE(b->signal.has_value());
  CHECK(b->signal->initial.at(0).decoded.command == 9);
  // delete the first, listen again
  r.press(Key::Up);
  r.press(Key::Right);
  REQUIRE(open(r, "Eliminar"));
  CHECK(r.shows("Capturas: 2"));
  REQUIRE(open(r, "Escuchar de nuevo"));
  CHECK(r.shows("Escuchando"));
  r.longPress(Key::Left);
  CHECK(r.topId() == std::string("home"));
  CHECK_FALSE(r.rx.running());
}

TEST_CASE("import a whole folder: every .ir file inside becomes a control of one new category") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/a.ir", irFile({{"Power", 8}, {"Vol_up", 2}}));
  r.card.add("/irdb/TVs/LG/b.ir", irFile({{"Power", 9}}));
  r.card.add("/irdb/TVs/LG/c.ir", "garbage");
  r.card.add("/irdb/TVs/LG/Sub/d.ir", irFile({{"Power", 10}}));  // not inside the folder itself
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "» Importar carpeta"));
  CHECK(r.topId() == std::string("import-folder-target"));
  r.press(Key::Ok);  // "+ LG": a new category named after the folder
  CHECK(r.topId() == std::string("import-folder"));
  r.advance(2000);
  CHECK(r.shows("Importados: 2"));
  CHECK(r.shows("Omitidos: 1"));
  REQUIRE(r.library.data().categories.size() == 1);
  CHECK(r.library.data().categories[0].name == "LG");
  CHECK(r.library.data().remotes.size() == 2);
  CHECK_FALSE(r.library.dirty());  // saved
}

TEST_CASE("home: six icons on one page, Up/Down switch between the two rows") {
  UiRig r;
  CHECK(r.display.shownBitmaps() == 6);
  CHECK(isSelected(r.screen(), "Controles"));
  r.press(Key::Down);
  CHECK(isSelected(r.screen(), "Be Gone"));
  r.press(Key::Down);   // wraps to the first row
  CHECK(isSelected(r.screen(), "Controles"));
  r.press(Key::Up);
  CHECK(isSelected(r.screen(), "Be Gone"));
  r.press(Key::Right);
  CHECK(isSelected(r.screen(), "Herramientas IR"));
  r.press(Key::Right);
  CHECK(isSelected(r.screen(), "Configuración"));
  r.press(Key::Right);  // Left/Right walk through all six and wrap
  CHECK(isSelected(r.screen(), "Controles"));
  r.press(Key::Right);
  CHECK(isSelected(r.screen(), "Aprender señal"));
  r.press(Key::Right);
  CHECK(isSelected(r.screen(), "Escuchar"));
}
