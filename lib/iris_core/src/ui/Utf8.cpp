#include "ui/Utf8.h"

namespace iris {

size_t utf8Length(std::string_view s) {
  size_t n = 0;
  for (char c : s) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
  }
  return n;
}

std::string utf8Truncate(std::string_view s, size_t maxCols) {
  size_t cols = 0, i = 0;
  while (i < s.size()) {
    size_t len = 1;
    while (i + len < s.size() && (static_cast<unsigned char>(s[i + len]) & 0xC0) == 0x80) ++len;
    if (cols == maxCols) break;
    ++cols;
    i += len;
  }
  return std::string(s.substr(0, i));
}

std::string utf8Justify(std::string_view left, std::string_view right, size_t cols) {
  const size_t rightLen = utf8Length(right);
  if (rightLen >= cols) return utf8Truncate(right, cols);
  const size_t room = rightLen ? cols - rightLen - 1 : cols;
  std::string out = utf8Truncate(left, room);
  size_t used = utf8Length(out);
  const size_t target = cols - rightLen;
  while (used < target) {
    out += ' ';
    ++used;
  }
  out += std::string(right);
  return out;
}

}  // namespace iris
