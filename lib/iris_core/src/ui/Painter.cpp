#include "ui/Painter.h"

#include "ui/Utf8.h"

namespace iris {

void Painter::mix(std::string_view s, int a, int b) {
  auto step = [this](uint8_t byte) { hash_ = (hash_ ^ byte) * 16777619u; };
  step(static_cast<uint8_t>(a));
  step(static_cast<uint8_t>(b));
  for (char c : s) step(static_cast<uint8_t>(c));
  step(0xFF);
}

void Painter::begin() {
  hash_ = 2166136261u;
  footerLocked_ = false;
  display_.clear();
}

void Painter::text(int col, int row, std::string_view s) {
  if (row < 0 || row >= kRows || col < 0 || col >= kCols) return;
  const std::string clipped = utf8Truncate(s, static_cast<size_t>(kCols - col));
  mix(clipped, col, row);
  display_.drawText(col * kCellW, row * kCellH, clipped.c_str(), FontSize::Small);
}

void Painter::title(std::string_view s) {
  text(0, 0, s);
  mix("rule", 0, 0);
  display_.drawHLine(0, kCellH, display_.width());  // visual tuning pending on the real panel
}

void Painter::item(int row, std::string_view label, std::string_view value, bool selected) {
  std::string line = std::string(selected ? ">" : " ") + " " + utf8Justify(label, value, kCols - 2);
  text(0, row, line);
}

void Painter::footer(std::string_view s) {
  if (!footerLocked_) text(0, kFooterRow, s);
}

void Painter::toastFooter(std::string_view s) {
  footerLocked_ = false;
  text(0, kFooterRow, s);
  footerLocked_ = true;
}

Status Painter::present() { return display_.flush(); }

}  // namespace iris
