// Helpers shared by the UI flow tests: navigate by label, type text with the character picker, fake captures.
#pragma once

#include <doctest.h>

#include <string>
#include <vector>

#include "Fixtures.h"
#include "UiRig.h"

namespace uihelp {

using namespace iris;
using namespace iris::testkit;

// Moves the cursor with Down until the row "> label" is highlighted. False if never found.
inline bool select(UiRig& r, const std::string& label) {
  for (int i = 0; i < 40; ++i) {
    if (r.screen().find("> " + label) != std::string::npos) return true;
    r.press(Key::Down);
  }
  return false;
}

inline bool open(UiRig& r, const std::string& label) {
  if (!select(r, label)) return false;
  r.press(Key::Ok);
  return true;
}

// Types text in the character picker: Up/Down choose, Right appends, OK finishes.
inline void typeText(UiRig& r, const std::string& text, bool finish = true) {
  static const std::vector<std::string> cs = [] {
    std::vector<std::string> v;
    for (char c = 'A'; c <= 'Z'; ++c) v.emplace_back(1, c);
    for (char c = 'a'; c <= 'z'; ++c) v.emplace_back(1, c);
    for (char c = '0'; c <= '9'; ++c) v.emplace_back(1, c);
    for (const char* s : {" ", "-", "+", ".", "_", "ñ", "Ñ", "á", "é", "í", "ó", "ú", "Á", "É", "Í", "Ó", "Ú"}) v.emplace_back(s);
    return v;
  }();
  size_t idx = 0;
  for (size_t i = 0; i < text.size();) {
    size_t len = 1;
    while (i + len < text.size() && (static_cast<unsigned char>(text[i + len]) & 0xC0) == 0x80) ++len;
    const std::string ch = text.substr(i, len);
    size_t target = 0;
    for (; target < cs.size(); ++target) {
      if (cs[target] == ch) break;
    }
    REQUIRE(target < cs.size());
    const size_t n = cs.size();
    const size_t down = (target + n - idx) % n;
    if (down <= n / 2) {
      for (size_t k = 0; k < down; ++k) r.press(Key::Down);
    } else {
      for (size_t k = 0; k < n - down; ++k) r.press(Key::Up);
    }
    idx = target;
    r.press(Key::Right);
    i += len;
  }
  if (finish) r.press(Key::Ok);
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
