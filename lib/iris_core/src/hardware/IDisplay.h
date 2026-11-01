#pragma once

#include <cstdint>

#include "Result.h"

namespace iris {

enum class FontSize : uint8_t { Small, Normal, Large };

// Minimal 1-bit display contract. Text is UTF-8 (Spanish accents / n-tilde must render).
// Drawing only touches the back buffer; flush() sends it to the panel.
class IDisplay {
 public:
  virtual ~IDisplay() = default;
  virtual Status begin() = 0;  // reports failure instead of hanging when the panel is absent
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void clear() = 0;
  virtual void drawText(int x, int y, const char* utf8, FontSize size) = 0;  // y = top of glyph row
  virtual void drawHLine(int x, int y, int length) = 0;
  virtual void drawRect(int x, int y, int w, int h, bool filled) = 0;
  virtual void setInverted(bool inverted) = 0;  // inverts the whole panel (selection feedback)
  virtual Status flush() = 0;
};

}  // namespace iris
