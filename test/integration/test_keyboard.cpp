#include <doctest.h>

#include "domain/Limits.h"
#include "domain/Naming.h"
#include "ui_helpers.h"

using namespace uihelp;

namespace {
TextInputScreen* keyboardOf(UiRig& r) { return dynamic_cast<TextInputScreen*>(r.ui.top()); }
}  // namespace

TEST_CASE("keyboard: every letter, digit and accent used in names has a key") {
  const char* chars[] = {"a", "z", "ñ", "Ñ", "A", "Z", "0", "9", ",", ".", "-", "á", "é", "í", "ó", "ú", "Á", "É",
                         "Í", "Ó", "Ú", "ü", "¿", "¡", "+", "_", "(", ")", "/", "@"};
  for (const char* ch : chars) {
    TextInputScreen::Cell cell;
    INFO("character " << ch);
    CHECK(TextInputScreen::locate(ch, cell));
  }
  TextInputScreen::Cell none;
  CHECK_FALSE(TextInputScreen::locate("€", none));
}

TEST_CASE("keyboard: a new name starts in upper case and each letter returns to lower case") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  kbOpen(r);
  auto* in = keyboardOf(r);
  REQUIRE(in);
  CHECK(in->page() == 1);  // upper-case page
  kbMoveTo(r, in, 1, 0);   // "A"
  r.press(Key::Ok);
  CHECK(in->text() == "A");
  CHECK(in->page() == 0);  // back to lower case
  kbMoveTo(r, in, 1, 1);   // "s"
  r.press(Key::Ok);
  CHECK(in->text() == "As");
}

TEST_CASE("keyboard: the page key cycles lower, upper and accents, and shows what comes next") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  kbOpen(r);
  auto* in = keyboardOf(r);
  REQUIRE(in);
  CHECK(in->page() == 1);
  kbMoveTo(r, in, TextInputScreen::kCtrlRow, 0);
  r.press(Key::Ok);
  CHECK(in->page() == 2);
  CHECK(r.shows("á"));
  r.press(Key::Ok);
  CHECK(in->page() == 0);
  CHECK(r.shows("ABC"));  // the page key announces the next page
}

TEST_CASE("keyboard: space, erase, long names and cancel") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  typeText(r, "Mi sala", false);
  auto* in = keyboardOf(r);
  REQUIRE(in);
  CHECK(in->text() == "Mi sala");
  kbErase(r, 5);
  CHECK(in->text() == "Mi");
  kbErase(r, 10);  // erasing an empty text is harmless
  CHECK(in->text().empty());
  typeText(r, "Niño", false);
  CHECK(in->text() == "Niño");
  kbErase(r, 1);
  CHECK(in->text() == "Niñ");
  kbErase(r, 1);   // removes the whole two-byte "ñ", never half of it
  CHECK(in->text() == "Ni");
  r.longPress(Key::Left);  // cancel
  CHECK(r.topId() == std::string("library"));
}

TEST_CASE("keyboard: the cursor wraps and the control row keeps a sensible column") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  kbOpen(r);
  auto* in = keyboardOf(r);
  REQUIRE(in);
  CHECK(in->row() == 0);
  CHECK(in->col() == 0);
  r.press(Key::Left);
  CHECK(in->col() == TextInputScreen::kKeyCols - 1);  // wraps to the last key
  r.press(Key::Up);
  CHECK(in->row() == TextInputScreen::kCtrlRow);      // up from the first row reaches the control row
  CHECK(in->col() < TextInputScreen::kCtrlCols);      // the column was mapped into the 4 control keys
  r.press(Key::Down);
  CHECK(in->row() == 0);
}

TEST_CASE("suggested names: pick one with OK, or open the keyboard with the last entry") {
  UiRig r;
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  auto* in = keyboardOf(r);
  REQUIRE(in);
  REQUIRE(in->inPresets());
  REQUIRE(in->presetList().size() >= 5);
  const std::string second = in->presetList()[1];
  CHECK(r.shows(in->presetList()[0]));
  CHECK_FALSE(r.shows("Escribir otro..."));  // the last entry is below the first screenful
  r.press(Key::Up);                          // wraps to "Escribir otro..."
  CHECK(r.shows("Escribir otro..."));
  r.press(Key::Down);                        // back to the first suggestion
  r.press(Key::Down);                        // second suggestion
  r.press(Key::Ok);
  REQUIRE(r.library.data().categories.size() == 1);
  CHECK(r.library.data().categories[0].name == second);
}

TEST_CASE("suggested names are valid names that fit the length limit, on all three prompts") {
  auto check = [](UiRig& r) {
    auto* in = keyboardOf(r);
    REQUIRE(in);
    REQUIRE(in->inPresets());
    for (const auto& name : in->presetList()) {
      INFO(name);
      CHECK(name.size() <= limits::kMaxNameBytes);
      CHECK(validateName(name).ok());
    }
  };
  {
    UiRig r;
    open(r, "Controles");
    open(r, "+ Nueva categoría");
    check(r);
  }
  {
    UiRig r;
    seed(r);
    open(r, "Controles");
    open(r, "TV");
    open(r, "+ Nuevo control");
    check(r);
  }
  {
    UiRig r;
    seed(r);
    open(r, "Controles");
    open(r, "TV");
    open(r, "Sala");
    open(r, "+ Nuevo botón");
    check(r);
  }
}
