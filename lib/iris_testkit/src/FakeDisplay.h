#pragma once

#include <array>
#include <string>
#include <vector>

#include "hardware/IDisplay.h"

namespace iris::testkit {

// Records what the app draws so tests can inspect the screen. Text is kept as text (grid()); shapes and bitmaps are
// also rasterised into a 128x64 buffer so a picture can be inspected (toPbm). Text glyphs are only approximated as
// 5x7 dither blocks in that picture: it shows the layout, not the real font.
class FakeDisplay : public IDisplay {
 public:
  struct TextOp {
    int x, y;
    std::string text;
    FontSize size;
  };

  Status begin() override {
    if (failBegin_) return Status::fail(Err::Unavailable, "display not found on I2C (injected)");
    began_ = true;
    return Status::success();
  }
  int width() const override { return 128; }
  int height() const override { return 64; }
  void clear() override {
    pending_.clear();
    pendingShapes_ = 0;
    pendingBitmaps_ = 0;
    pendingPx_.fill(false);
    color_ = true;
  }
  void drawText(int x, int y, const char* utf8, FontSize size) override {
    pending_.push_back(TextOp{x, y, utf8 ? utf8 : "", size});
    int cx = x;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8 ? utf8 : ""); *p; ++p) {
      if ((*p & 0xC0) == 0x80) continue;  // continuation byte: same glyph
      if (*p != ' ') {
        for (int gy = 0; gy < 7; ++gy)
          for (int gx = 0; gx < 5; ++gx)
            if ((gx + gy) % 2 == 0 || gy == 0) plot(cx + gx, y + gy, color_);
      }
      cx += 6;
    }
  }
  void drawHLine(int x, int y, int length) override {
    ++pendingShapes_;
    for (int i = 0; i < length; ++i) plot(x + i, y, color_);
  }
  void drawRect(int x, int y, int w, int h, bool filled) override {
    ++pendingShapes_;
    for (int j = 0; j < h; ++j)
      for (int i = 0; i < w; ++i)
        if (filled || i == 0 || j == 0 || i == w - 1 || j == h - 1) plot(x + i, y + j, color_);
  }
  void drawBitmap(int x, int y, int w, int h, const uint8_t* xbm) override {
    ++pendingBitmaps_;
    const int stride = (w + 7) / 8;
    for (int j = 0; j < h; ++j)
      for (int i = 0; i < w; ++i)
        if ((xbm[j * stride + i / 8] >> (i % 8)) & 1) plot(x + i, y + j, color_);
  }
  void setDrawColor(bool lit) override { color_ = lit; }
  void setInverted(bool inverted) override { pendingInverted_ = inverted; }
  Status flush() override {
    if (failFlush_) return Status::fail(Err::StorageFailure, "display flush failed (injected)");
    shown_ = pending_;
    shownShapes_ = pendingShapes_;
    shownBitmaps_ = pendingBitmaps_;
    shownPx_ = pendingPx_;
    inverted_ = pendingInverted_;
    ++flushCount_;
    return Status::success();
  }

  // Inspection (what the user would currently see)
  const std::vector<TextOp>& shown() const { return shown_; }
  bool showsText(const std::string& needle) const {
    for (const auto& t : shown_) {
      if (t.text.find(needle) != std::string::npos) return true;
    }
    return false;
  }
  std::string dump() const {
    std::string out;
    for (const auto& t : shown_) out += t.text + "\n";
    return out;
  }
  // The 21x8 character grid as the user would read it (text cells of 6x8 px), trailing blanks trimmed.
  std::string grid() const {
    constexpr int kCols = 21, kRows = 8;
    std::vector<std::vector<std::string>> cells(kRows, std::vector<std::string>(kCols, " "));
    for (const auto& t : shown_) {
      int row = t.y / 8, col = t.x / 6;
      for (size_t i = 0; i < t.text.size() && row >= 0 && row < kRows; ) {
        size_t len = 1;
        while (i + len < t.text.size() && (static_cast<unsigned char>(t.text[i + len]) & 0xC0) == 0x80) ++len;
        if (col >= 0 && col < kCols) cells[static_cast<size_t>(row)][static_cast<size_t>(col)] = t.text.substr(i, len);
        ++col;
        i += len;
      }
    }
    std::string out;
    for (const auto& r : cells) {
      std::string line;
      for (const auto& c : r) line += c;
      while (!line.empty() && line.back() == ' ') line.pop_back();
      out += line + "\n";
    }
    return out;
  }
  bool inverted() const { return inverted_; }
  int shownShapes() const { return shownShapes_; }
  int shownBitmaps() const { return shownBitmaps_; }
  int flushCount() const { return flushCount_; }
  bool began() const { return began_; }

  bool pixel(int x, int y) const { return x >= 0 && x < 128 && y >= 0 && y < 64 && shownPx_[static_cast<size_t>(y * 128 + x)]; }
  // Portable bitmap (P1, text) of what is shown; open it with any image viewer or convert with tools/pbm_to_png.py.
  std::string toPbm() const {
    std::string out = "P1\n128 64\n";
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 128; ++x) out += pixel(x, y) ? '1' : '0';
      out += '\n';
    }
    return out;
  }

  void setFailBegin(bool f) { failBegin_ = f; }
  void setFailFlush(bool f) { failFlush_ = f; }

 private:
  void plot(int x, int y, bool lit) {
    if (x >= 0 && x < 128 && y >= 0 && y < 64) pendingPx_[static_cast<size_t>(y * 128 + x)] = lit;
  }

  std::vector<TextOp> pending_, shown_;
  std::array<bool, 128 * 64> pendingPx_{}, shownPx_{};
  int pendingShapes_ = 0, shownShapes_ = 0, pendingBitmaps_ = 0, shownBitmaps_ = 0, flushCount_ = 0;
  bool color_ = true;
  bool pendingInverted_ = false, inverted_ = false;
  bool began_ = false, failBegin_ = false, failFlush_ = false;
};

}  // namespace iris::testkit
