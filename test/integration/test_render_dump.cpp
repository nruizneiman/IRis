// Manual aid, not a check: when IRIS_DUMP_DIR is set, writes the simulated screens as .pbm pictures so the layout can be
// looked at (convert with tools/pbm_to_png.py). Text is approximated by dither blocks there, icons and bars are exact.
#include <doctest.h>

#include <cstdlib>
#include <fstream>
#include <string>

#include "ui_helpers.h"

using namespace uihelp;

namespace {
void dump(UiRig& r, const std::string& dir, const std::string& name) {
  std::ofstream(dir + "/" + name + ".pbm", std::ios::binary) << r.display.toPbm();
}
}  // namespace

TEST_CASE("render dump (only when IRIS_DUMP_DIR is set)") {
  const char* dir = std::getenv("IRIS_DUMP_DIR");
  if (!dir) return;
  UiRig r;
  dump(r, dir, "home_0");
  r.battery.set(3770);
  r.advance(300);
  dump(r, dir, "home_battery");
  r.press(Key::Down);
  dump(r, dir, "home_battery_row2");
  r.press(Key::Down);  // back on the first icon
  r.battery.set(std::nullopt);
  r.advance(300);
  r.press(Key::Right);
  r.press(Key::Right);
  dump(r, dir, "home_2");
  r.press(Key::Down);
  dump(r, dir, "home_row2");
  open(r, "Herramientas IR");
  dump(r, dir, "tools_list");
  r.longPress(Key::Left);
  open(r, "Controles");
  open(r, "+ Nueva categoría");
  dump(r, dir, "presets");
  kbOpen(r);
  auto* in = dynamic_cast<TextInputScreen*>(r.ui.top());
  kbMoveTo(r, in, 0, 3);
  r.press(Key::Ok);
  kbMoveTo(r, in, 1, 5);
  dump(r, dir, "keyboard");
  CHECK(true);
}
