// Interface language, kept apart from the UI so the settings can store it.
#pragma once

#include <cstdint>

namespace iris {

enum class Lang : uint8_t { En = 0, Es = 1 };

inline constexpr Lang kDefaultLang = Lang::En;
inline constexpr unsigned kLangCount = 2;

}  // namespace iris
