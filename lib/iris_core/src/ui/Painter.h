// Character-grid drawing on top of IDisplay: 21 columns x 8 rows (6x8 pixel cells).
// Row 0 = title, rows 1..6 = content, row 7 = footer / toast. A hash of everything drawn lets the
// app skip redundant (slow) I2C flushes when the picture did not change.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hardware/IDisplay.h"

namespace iris {

class Painter {
 public:
  static constexpr int kCols = 21;
  static constexpr int kRows = 8;
  static constexpr int kCellW = 6;
  static constexpr int kCellH = 8;
  static constexpr int kFirstContentRow = 1;
  static constexpr int kContentRows = 6;
  static constexpr int kFooterRow = 7;

  explicit Painter(IDisplay& display) : display_(display) {}

  void begin();  // clears the back buffer and the hash
  void text(int col, int row, std::string_view s);  // clipped to the grid
  void title(std::string_view s);
  // One list row: marker, label, optional right-aligned value.
  void item(int row, std::string_view label, std::string_view value, bool selected);
  void footer(std::string_view s);  // ignored while a toast owns the footer row
  void footerCentered(std::string_view s);  // same, centred (the home grid shows the selected name here)
  // Thin scroll indicator in the two right-most pixel columns of the content rows; drawn only when the list does not fit.
  void scrollbar(size_t total, size_t first, size_t visible);
  // A key of the on-screen keyboard at pixel position (x, y): `w` px wide, 8 px high, label centred; the selected one is
  // drawn as a lit box with dark text.
  void keyCap(int x, int y, int w, std::string_view label, bool selected);
  void bitmap(int x, int y, int w, int h, const uint8_t* xbm);  // XBM, see IDisplay::drawBitmap
  void frame(int x, int y, int w, int h);                       // 1 px outline
  // Battery outline with 0..4 bars, dark on the lit title bar, in its right-most 17 pixels. Call after title().
  void batteryGauge(int segments);
  // Draws a transient message on the footer row and keeps the screen from drawing over it.
  void toastFooter(std::string_view s);
  uint32_t hash() const { return hash_; }
  Status present();  // flush

 private:
  void mix(std::string_view s, int a, int b);

  IDisplay& display_;
  uint32_t hash_ = 2166136261u;
  bool footerLocked_ = false;
};

}  // namespace iris
