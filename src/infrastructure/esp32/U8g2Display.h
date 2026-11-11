#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "board_pins.h"
#include "hardware/IDisplay.h"

// OLED 128x64 over I2C. The controller family is a build-time choice because the real panel is
// unconfirmed (see docs/hardware/bring-up.md): -DIRIS_DISPLAY_SH1106 or -DIRIS_DISPLAY_SSD1306.
#if defined(IRIS_DISPLAY_SSD1306)
using IrisPanel = U8G2_SSD1306_128X64_NONAME_F_HW_I2C;
static constexpr const char* kIrisPanelName = "SSD1306 (probado)";  // identified with display_probe
#elif defined(IRIS_DISPLAY_SH1106)
using IrisPanel = U8G2_SH1106_128X64_NONAME_F_HW_I2C;
static constexpr const char* kIrisPanelName = "SH1106 (sin verificar)";
#else
#error "Define IRIS_DISPLAY_SH1106 or IRIS_DISPLAY_SSD1306 (see platformio.ini)"
#endif

class U8g2Display : public iris::IDisplay {
 public:
  // Never blocks forever: a missing panel is reported as an error and retried by flush().
  iris::Status begin() override {
    Wire.begin(board::kI2cSda, board::kI2cScl);
    Wire.setClock(400000);
    addr_ = 0;
    for (uint8_t candidate : {0x3C, 0x3D}) {
      if (responds(candidate)) {
        addr_ = candidate;
        break;
      }
    }
    if (addr_ == 0) return iris::Status::fail(iris::Err::Unavailable, "no I2C device at 0x3C/0x3D");
    u8g2_.setI2CAddress(static_cast<uint8_t>(addr_ << 1));  // U8g2 uses the 8-bit form
    if (!u8g2_.begin()) return iris::Status::fail(iris::Err::Unavailable, "display init failed");
    u8g2_.setFont(u8g2_font_5x8_tf);  // Latin-1 glyphs (accents, n-tilde, inverted ! and ?): verify on the panel
    u8g2_.setFontPosTop();
    u8g2_.setFontMode(1);  // transparent glyphs: dark text over a lit bar only erases the glyph pixels
    u8g2_.clearBuffer();
    ready_ = true;
    return iris::Status::success();
  }

  int width() const override { return 128; }
  int height() const override { return 64; }
  void clear() override {
    if (ready_) {
      u8g2_.clearBuffer();
      u8g2_.setDrawColor(1);  // the previous frame may have ended in dark-on-lit mode
    }
  }

  // The UI places text on a 6x8 pixel grid; glyphs are drawn one by one so the 5 px wide font
  // keeps that spacing regardless of the string. UTF-8 is decoded for 1- and 2-byte sequences
  // (all of Latin-1); anything longer is drawn as '?'.
  void drawText(int x, int y, const char* utf8, iris::FontSize) override {
    if (!ready_ || !utf8) return;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8);
    while (*p) {
      uint32_t cp = *p++;
      if (cp >= 0xC0) {
        const int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : 1;
        if (extra == 1 && (*p & 0xC0) == 0x80) {
          cp = ((cp & 0x1F) << 6) | (*p & 0x3F);
          ++p;
        } else {
          cp = '?';
          for (int i = 0; i < extra && (*p & 0xC0) == 0x80; ++i) ++p;
        }
      }
      u8g2_.drawGlyph(x, y, static_cast<uint16_t>(cp));
      x += 6;
    }
  }

  void drawHLine(int x, int y, int length) override {
    if (ready_) u8g2_.drawHLine(x, y, length);
  }
  void drawRect(int x, int y, int w, int h, bool filled) override {
    if (!ready_) return;
    if (filled) u8g2_.drawBox(x, y, w, h);
    else u8g2_.drawFrame(x, y, w, h);
  }
  void drawBitmap(int x, int y, int w, int h, const uint8_t* xbm) override {
    if (ready_ && xbm) u8g2_.drawXBM(static_cast<u8g2_uint_t>(x), static_cast<u8g2_uint_t>(y), static_cast<u8g2_uint_t>(w),
                                     static_cast<u8g2_uint_t>(h), xbm);
  }
  void setDrawColor(bool lit) override {
    if (ready_) u8g2_.setDrawColor(lit ? 1 : 0);
  }
  void setInverted(bool inverted) override {
    if (ready_ && inverted != inverted_) {
      inverted_ = inverted;
      u8g2_.sendF("c", inverted ? 0xA7 : 0xA6);  // SSD1306/SH1106 inverse / normal display command
    }
  }

  iris::Status flush() override {
    if (!ready_) {
      iris::Status st = begin();  // lazy retry: the panel may have been connected later
      if (!st.ok()) return st;
    }
    if (!responds(addr_)) {
      ready_ = false;
      return iris::Status::fail(iris::Err::Unavailable, "display stopped responding");
    }
    u8g2_.sendBuffer();
    return iris::Status::success();
  }

 private:
  static bool responds(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }

  IrisPanel u8g2_{U8G2_R0, U8X8_PIN_NONE};
  uint8_t addr_ = 0;
  bool ready_ = false;
  bool inverted_ = false;
};
