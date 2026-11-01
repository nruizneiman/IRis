// Helpers shared by the UI flow tests: navigate by label, type text with the character picker, fake captures.
#pragma once

#include <doctest.h>

#include <string>
#include <vector>

#include "Fixtures.h"
#include "UiRig.h"
#include "ui/Widgets.h"

namespace uihelp {

using namespace iris;
using namespace iris::testkit;

// True when `label` is the selected entry: a list row marked "> label", or the name centred on the last row of the
// icon grid (home screen).
inline bool isSelected(const std::string& screen, const std::string& label) {
  if (screen.find("> " + label) != std::string::npos) return true;
  std::string last = screen;
  while (!last.empty() && last.back() == '\n') last.pop_back();
  const size_t nl = last.rfind('\n');
  last = nl == std::string::npos ? last : last.substr(nl + 1);
  size_t a = last.find_first_not_of(' ');
  return a != std::string::npos && last.substr(a) == label;
}

// Moves the cursor with Down until `label` is selected. False if never found.
inline bool select(UiRig& r, const std::string& label) {
  for (int i = 0; i < 40; ++i) {
    if (isSelected(r.screen(), label)) return true;
    // On the icon grid Down jumps a whole row (it would only alternate between two icons): walk with Right there.
    r.press(r.topId() == std::string("home") ? Key::Right : Key::Down);
  }
  return false;
}

inline bool open(UiRig& r, const std::string& label) {
  if (!select(r, label)) return false;
  r.press(Key::Ok);
  return true;
}

// Moves the keyboard cursor to (row, col) using the arrow keys, reading the real cursor after every press.
inline void kbMoveTo(UiRig& r, TextInputScreen* in, int row, int col) {
  for (int guard = 0; in->row() != row && guard < 8; ++guard) r.press(Key::Down);
  for (int guard = 0; in->col() != col && guard < 12; ++guard) r.press(Key::Right);
  REQUIRE(in->row() == row);
  REQUIRE(in->col() == col);
}

// Types text on the on-screen keyboard: switches page when needed, moves to each key, presses OK, finishes with the
// "Listo" key. When the screen starts with its list of suggested names, picks the one equal to the text if there
// is one (that finishes the screen), otherwise opens the keyboard.
inline void typeText(UiRig& r, const std::string& text, bool finish = true) {
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  REQUIRE(in != nullptr);
  if (in->inPresets()) {
    const auto& list = in->presetList();
    size_t want = list.size();  // "Escribir otro..."
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i] == text) want = i;
    for (size_t i = 0; i < want; ++i) r.press(Key::Down);
    r.press(Key::Ok);
    if (want < list.size()) return;  // a suggested name was chosen: the screen is done
    in = dynamic_cast<TextInputScreen*>(r.ui.top());
    REQUIRE(in != nullptr);
  }
  for (size_t i = 0; i < text.size();) {
    size_t len = 1;
    while (i + len < text.size() && (static_cast<unsigned char>(text[i + len]) & 0xC0) == 0x80) ++len;
    const std::string ch = text.substr(i, len);
    i += len;
    if (ch == " ") {
      kbMoveTo(r, in, TextInputScreen::kCtrlRow, 1);
      r.press(Key::Ok);
      continue;
    }
    TextInputScreen::Cell cell;
    REQUIRE(TextInputScreen::locate(ch, cell));
    for (int guard = 0; in->page() != cell.page && guard < 4; ++guard) {
      kbMoveTo(r, in, TextInputScreen::kCtrlRow, 0);
      r.press(Key::Ok);  // next page
    }
    REQUIRE(in->page() == cell.page);
    kbMoveTo(r, in, cell.row, cell.col);
    r.press(Key::Ok);
  }
  if (finish) {
    kbMoveTo(r, in, TextInputScreen::kCtrlRow, 3);
    r.press(Key::Ok);
  }
}

// From the list of suggested names, opens the keyboard (the "Escribir otro..." entry). Does nothing on a keyboard.
inline void kbOpen(UiRig& r) {
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  REQUIRE(in != nullptr);
  if (!in->inPresets()) return;
  for (size_t i = 0; i < in->presetList().size(); ++i) r.press(Key::Down);
  r.press(Key::Ok);
  REQUIRE_FALSE(in->inPresets());
}

// Presses the "Borrar" key n times / the "Listo" key of the keyboard that is on top.
inline void kbErase(UiRig& r, int n) {
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  REQUIRE(in != nullptr);
  kbMoveTo(r, in, TextInputScreen::kCtrlRow, 2);
  for (int i = 0; i < n; ++i) r.press(Key::Ok);
}
inline void kbFinish(UiRig& r) {
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  REQUIRE(in != nullptr);
  kbMoveTo(r, in, TextInputScreen::kCtrlRow, 3);
  r.press(Key::Ok);
}

// A capture as a receiver adapter would hand it over (SYNTHETIC timings, decoded part supplied by the test).
inline ReceivedCapture necCapture(uint32_t addr = 0, uint32_t cmd = 8) {
  ReceivedCapture c;
  const uint32_t data = addr | ((~addr & 0xFFu) << 8) | (cmd << 16) | ((~cmd & 0xFFu) << 24);
  c.raw.durationsUs = syntheticNecFrame(data);
  DecodedFrame d;
  d.protocol = Protocol::Nec;
  d.address = addr;
  d.command = cmd;
  d.bits = 32;
  c.decoded = d;
  return c;
}

// Creates category "TV", remote "Sala" and button "Power" directly in the library.
struct Ids {
  Id cat, remote, button;
};
inline Ids seed(UiRig& r) {
  Ids ids{};
  ids.cat = r.library.addCategory("TV").value();
  ids.remote = r.library.addRemote("Sala", ids.cat).value();
  ids.button = r.library.addButton(ids.remote, "Power").value();
  r.library.markClean();
  return ids;
}

}  // namespace uihelp
