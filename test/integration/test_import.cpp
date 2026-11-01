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

// Opens Importar > TVs > LG > "LG" and stops on the preview screen.
void openLgPreview(UiRig& r) {
  REQUIRE(open(r, "Importar"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "LG/"));
  REQUIRE(open(r, "LG"));
  REQUIRE(r.topId() == std::string("import-preview"));
}

}  // namespace

TEST_CASE("import: without a card the home explains it and stays put") {
  UiRig r;
  r.card.setPresent(false);
  open(r, "Importar");
  CHECK(r.topId() == std::string("home"));
  CHECK(r.shows("Sin tarjeta SD"));
}

TEST_CASE("import: browse the card, preview a real IRDB file and create a control from it") {
  UiRig r;
  r.card.add("/irdb/TVs/LG/LG.ir", fixture("irdb_LG.ir"));
  r.card.add("/irdb/TVs/LG/notes.txt", "not a remote");  // ignored: not a .ir file
  openLgPreview(r);
  CHECK(r.shows("Botones: 6"));
  CHECK_FALSE(r.shows("Sin soporte"));
  r.press(Key::Ok);  // import
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
    openLgPreview(r);
    r.press(Key::Ok);
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
  REQUIRE(open(r, "Importar"));
  REQUIRE(open(r, "TVs/"));
  REQUIRE(open(r, "Dup/"));
  REQUIRE(open(r, "Dup"));
  r.press(Key::Ok);
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

  REQUIRE(open(r, "Importar"));
  REQUIRE(open(r, "Misc/"));
  REQUIRE(open(r, "Odd/"));
  REQUIRE(open(r, "Odd"));
  CHECK(r.shows("Botones: 0"));
  CHECK(r.shows("Sin soporte: 1"));
  r.press(Key::Ok);
  CHECK(r.shows("Nada para importar"));
  CHECK(r.topId() == std::string("import-preview"));
  r.press(Key::Left);
  r.press(Key::Left);
  REQUIRE(open(r, "Bad/"));
  REQUIRE(open(r, "Bad"));
  CHECK(r.shows("No es un archivo IRDB"));
  r.press(Key::Ok);
  CHECK(r.shows("Nada para importar"));
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
