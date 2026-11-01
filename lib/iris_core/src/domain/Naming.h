#pragma once

#include <string>
#include <string_view>

#include "Result.h"

namespace iris {

// Trims ASCII whitespace at both ends.
std::string trimName(std::string_view name);
// Non-empty, <= kMaxNameBytes UTF-8 bytes, valid UTF-8, no control characters.
Status validateName(std::string_view name);
// Case-insensitive for ASCII letters only; other bytes compare exactly.
bool namesEqual(std::string_view a, std::string_view b);

}  // namespace iris
