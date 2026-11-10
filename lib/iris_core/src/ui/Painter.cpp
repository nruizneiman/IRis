#include "ui/Painter.h"

#include <algorithm>

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

void Painter::text(int col, int row, std::string_view s) { rawText(col, row, translate(lang_, s)); }

void Painter::rawText(int col, int row, std::string_view s) {
  if (row < 0 || row >= kRows || col < 0 || col >= kCols) return;
  const std::string clipped = utf8Truncate(s, static_cast<size_t>(kCols - col));
  mix(clipped, col, row);
  display_.drawText(col * kCellW, row * kCellH, clipped.c_str(), FontSize::Small);
}

// Title bar: a lit bar across the panel with dark text, like the reference menu look the tester asked for.
void Painter::title(std::string_view s) {
  mix("bar", 0, 0);
  display_.setDrawColor(true);
  display_.drawRect(0, 0, display_.width(), kCellH, true);
  display_.setDrawColor(false);
  text(0, 0, s);
  display_.setDrawColor(true);
}

// The selected row is a lit bar with dark text. The ">" marker stays in the text: it is what the tests and the
// simulator read, and it still tells the selection when the bar is hard to see.
void Painter::item(int row, std::string_view label, std::string_view value, bool selected) {
  std::string line = std::string(selected ? ">" : " ") + " " + utf8Justify(translate(lang_, label), translate(lang_, value), kCols - 2);
  if (selected && row >= 0 && row < kRows) {
    mix("sel", 0, row);
    display_.setDrawColor(true);
    display_.drawRect(0, row * kCellH, kCols * kCellW - 1, kCellH, true);  // 125 px: leaves the scrollbar columns free
    display_.setDrawColor(false);
    rawText(0, row, line);
    display_.setDrawColor(true);
  } else {
    rawText(0, row, line);
  }
}

void Painter::footer(std::string_view s) {
  if (!footerLocked_) text(0, kFooterRow, s);
}

void Painter::footerCentered(std::string_view s) {
  if (footerLocked_) return;
  const std::string shown = translate(lang_, s);
  const size_t len = utf8Length(shown);
  const int col = len >= static_cast<size_t>(kCols) ? 0 : static_cast<int>((static_cast<size_t>(kCols) - len) / 2);
  rawText(col, kFooterRow, shown);
}

void Painter::scrollbar(size_t total, size_t first, size_t visible) {
  if (visible == 0 || total <= visible) return;
  constexpr int kTop = kFirstContentRow * kCellH;
  constexpr int kHeight = kContentRows * kCellH;
  const int thumb = static_cast<int>(std::max<size_t>(4, kHeight * visible / total));
  const int span = kHeight - thumb;
  const size_t last = total - visible;
  const int y = kTop + static_cast<int>(last == 0 ? 0 : span * std::min(first, last) / last);
  mix("scroll", static_cast<int>(total), static_cast<int>(first));
  display_.setDrawColor(true);
  display_.drawRect(display_.width() - 1, kTop, 1, kHeight, true);  // track
  display_.drawRect(display_.width() - 2, y, 2, thumb, true);        // thumb
}

void Painter::keyCap(int x, int y, int w, std::string_view label, bool selected) {
  const std::string clipped = utf8Truncate(translate(lang_, label), static_cast<size_t>(std::max(1, w / kCellW)));
  const int textW = static_cast<int>(utf8Length(clipped)) * kCellW;
  const int tx = x + std::max(0, (w - textW) / 2);
  mix(clipped, x, y);
  mix(selected ? "capS" : "cap", w, 0);
  if (selected) {
    display_.setDrawColor(true);
    display_.drawRect(x, y, w, kCellH, true);
    display_.setDrawColor(false);
  }
  display_.drawText(tx, y, clipped.c_str(), FontSize::Small);
  display_.setDrawColor(true);
}

void Painter::bitmap(int x, int y, int w, int h, const uint8_t* xbm) {
  mix("bmp", x, y);
  display_.drawBitmap(x, y, w, h, xbm);
}

void Painter::frame(int x, int y, int w, int h) {
  mix("frame", x, y);
  display_.drawRect(x, y, w, h, false);
}

void Painter::batteryGauge(int segments) {
  constexpr int kBodyW = 15, kNubW = 2, kSegPitch = 3;
  const int x = display_.width() - kBodyW - kNubW - 1;
  mix("battery", x, segments);
  display_.setDrawColor(false);
  display_.drawRect(x, 1, kBodyW, 6, false);
  display_.drawRect(x + kBodyW, 3, kNubW, 2, true);
  for (int i = 0; i < segments && i < 4; ++i) display_.drawRect(x + 2 + i * kSegPitch, 2, 2, 4, true);
  display_.setDrawColor(true);
}

void Painter::pageDots(int pages, int current) {
  constexpr int kDot = 3, kStep = 5, kY = 59;
  const int x0 = display_.width() - 2 - pages * kStep;
  mix("dots", pages, current);
  display_.setDrawColor(true);
  for (int i = 0; i < pages; ++i) display_.drawRect(x0 + i * kStep, kY, kDot, kDot, i == current);
}

void Painter::toastFooter(std::string_view s) {
  footerLocked_ = false;
  text(0, kFooterRow, s);
  footerLocked_ = true;
}

Status Painter::present() { return display_.flush(); }

}  // namespace iris
