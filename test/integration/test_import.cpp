#include <doctest.h>

#include <fstream>
#include <sstream>

#include "app/SignalBuilder.h"
#include "ui_helpers.h"

using namespace uihelp;

namespace {

std::string fixture(const char* name) {
  std::ifstream in(std::string(IRIS_FIXTURE_DIR) + "/" + name, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Opens Controles > TVs > LG > "LG" (the control read from the card) and its menu, and chooses Importar.
void importLg(UiRig& r) {
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "LG"));
  REQUIRE(r.topId() == std::string("card-remote"));
  r.press(Key::Right);
  REQUIRE(open(r, "Importar"));
}

}  // namespace

TEST_CASE("import: browse the card, open a real IRDB file and create a control from it") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/LG.ir", fixture("irdb_LG.ir"));
  r.card.add("/irdb/TVs/LG/notes.txt", "not a remote");  // ignored: not a .ir file
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  CHECK_FALSE(r.shows("notes"));
  REQUIRE(open(r, "LG"));
  CHECK(r.shows("Power"));  // the control can be tried before it is imported
  r.press(Key::Right);
  CHECK_FALSE(r.shows("Sin soporte"));
  REQUIRE(open(r, "Importar"));
  CHECK(r.topId() == std::string("import-category"));
  open(r, "+ Nueva categoría");
  typeText(r, "TV");  // a suggested name
  REQUIRE(r.library.data().remotes.size() == 1);
  const Remote& rem = r.library.data().remotes[0];
  CHECK(rem.name == "LG");
  REQUIRE(rem.buttons.size() == 6);
  CHECK(rem.buttons[0].name == "Power");
  REQUIRE(rem.buttons[0].signal.has_value());
  CHECK(rem.buttons[0].signal->initial.at(0).decoded.protocol == Protocol::Nec);
  CHECK(rem.buttons[0].signal->initial.at(0).decoded.address == 0x04);
  CHECK(r.library.data().categories.size() == 1);
  CHECK(r.library.data().categories[0].name == "TV");
  CHECK_FALSE(r.library.dirty());                // saved
  CHECK(r.topId() == std::string("remote"));     // opens on the new control
  CHECK(r.ui.depth() == 2);                      // home + the control: the browsing screens are gone
}

TEST_CASE("import: importing the same file twice keeps both, the second named 'LG 2'") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/LG.ir", fixture("irdb_LG.ir"));
  const Id cat = r.library.addCategory("TV").value();
  for (int i = 0; i < 2; ++i) {
    importLg(r);
    REQUIRE(open(r, "TV"));
    REQUIRE(r.library.data().remotes.size() == static_cast<size_t>(i + 1));
    r.longPress(Key::Left);  // back to home
  }
  CHECK(r.library.data().remotes[0].name == "LG");
  CHECK(r.library.data().remotes[1].name == "LG 2");
  CHECK(r.library.data().remotes[1].categoryId == cat);
}

TEST_CASE("import: duplicate button names inside one file are made unique") {
  UiRig r;
  const std::string text =
      "Filetype: IR signals file\nVersion: 1\n#\n"
      "name: Power\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: 08 00 00 00\n#\n"
      "name: Power\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: 09 00 00 00\n";
  r.card.add("/irdb/TVs/Dup/Dup.ir", text);
  const Id cat = r.library.addCategory("TV").value();
  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "Dup/"));
  REQUIRE(open(r, "Dup"));
  r.press(Key::Right);
  REQUIRE(open(r, "Importar"));
  REQUIRE(open(r, "TV"));
  REQUIRE(r.library.data().remotes.size() == 1);
  const auto& b = r.library.data().remotes[0].buttons;
  REQUIRE(b.size() == 2);
  CHECK(b[0].name == "Power");
  CHECK(b[1].name == "Power 2");
  (void)cat;
}

TEST_CASE("import: a file with nothing convertible, and a file that is not IRDB, import nothing") {
  UiRig r;
  r.card.add("/irdb/Misc/Odd/Odd.ir",
             "Filetype: IR signals file\nVersion: 1\n#\nname: A\ntype: parsed\nprotocol: RC6\naddress: 00 00 00 00\ncommand: 0C 00 00 00\n");
  r.card.add("/irdb/Misc/Bad/Bad.ir", "this is not an IRDB file\n");
  r.library.addCategory("TV");

  REQUIRE(open(r, "Controles"));
  REQUIRE(open(r, "Misc/"));
  REQUIRE(open(r, "Odd/"));
  REQUIRE(open(r, "Odd"));
  CHECK(r.shows("Sin soporte: 1"));  // no usable button: the screen says why
  r.press(Key::Right);
  CHECK(r.shows("Sin soporte: 1"));  // and so does the menu
  REQUIRE(open(r, "Importar"));
  CHECK(r.shows("Nada para importar"));
  CHECK(r.topId() == std::string("card-remote-menu"));
  r.press(Key::Left);
  r.press(Key::Left);
  r.press(Key::Left);
  REQUIRE(open(r, "Bad/"));
  REQUIRE(open(r, "Bad"));
  CHECK(r.shows("No es un archivo IR"));  // the row is cut at the screen width
  r.press(Key::Right);  // no menu for a file that could not be read
  CHECK(r.topId() == std::string("card-remote"));
  CHECK(r.library.data().remotes.empty());
}

TEST_CASE("import: a failure part-way leaves no half-built control behind") {
  RemoteLibrary lib;
  const Id cat = lib.addCategory("TV").value();
  std::vector<std::pair<std::string, IrSignal>> buttons;
  const IrSignal good = buildDecodedSignal(Protocol::Nec, 1, 2, 32).value();
  buttons.emplace_back("A", good);
  buttons.emplace_back("A", good);  // duplicate name: the second addButton fails
  lib.markClean();
  auto r = lib.importRemote("X", cat, buttons);
  CHECK_FALSE(r.ok());
  CHECK(lib.data().remotes.empty());
  CHECK_FALSE(lib.dirty());
}
