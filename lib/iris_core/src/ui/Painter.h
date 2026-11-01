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
