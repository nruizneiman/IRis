#pragma once

#include <string>
#include <vector>

#include "hardware/IDisplay.h"

namespace iris::testkit {

// Records what the app draws so tests can inspect the screen. It does NOT reproduce pixels.
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
  void clear() override { pending_.clear(); pendingShapes_ = 0; }
  void drawText(int x, int y, const char* utf8, FontSize size) override {
    pending_.push_back(TextOp{x, y, utf8 ? utf8 : "", size});
  }
  void drawHLine(int, int, int) override { ++pendingShapes_; }
  void drawRect(int, int, int, int, bool) override { ++pendingShapes_; }
  void setInverted(bool inverted) override { pendingInverted_ = inverted; }
  Status flush() override {
    if (failFlush_) return Status::fail(Err::StorageFailure, "display flush failed (injected)");
    shown_ = pending_;
    shownShapes_ = pendingShapes_;
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
  int flushCount() const { return flushCount_; }
  bool began() const { return began_; }

  void setFailBegin(bool f) { failBegin_ = f; }
  void setFailFlush(bool f) { failFlush_ = f; }

 private:
  std::vector<TextOp> pending_, shown_;
  int pendingShapes_ = 0, shownShapes_ = 0, flushCount_ = 0;
  bool pendingInverted_ = false, inverted_ = false;
  bool began_ = false, failBegin_ = false, failFlush_ = false;
};

}  // namespace iris::testkit
