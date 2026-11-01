#pragma once

#include <string>
#include <string_view>

namespace iris {

// Number of code points (continuation bytes are not counted). Input is assumed valid UTF-8.
size_t utf8Length(std::string_view s);
// First `maxCols` code points; never cuts a multi-byte character.
std::string utf8Truncate(std::string_view s, size_t maxCols);
// "left" ... "right" on one line of exactly `cols` columns (left is truncated first).
std::string utf8Justify(std::string_view left, std::string_view right, size_t cols);

}  // namespace iris
