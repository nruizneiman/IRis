// Interface language. The UI is written with Spanish text; for English it is translated when it is drawn, through a
// catalog (Spanish text -> English text), so the language can change while the device runs and the screens need no
// rebuild. A string with no catalog entry is shown as written, which is also what happens to every name the user
// typed or imported.
#pragma once

#include <string>
#include <string_view>

#include "domain/Lang.h"

namespace iris {

const char* langName(Lang l);  // "English" / "Español": always in its own language
Lang nextLang(Lang l);         // the next language in the selector (wraps)

// Text for `lang`. For Spanish it returns the input. For English it tries, in this order:
//   1. the whole string in the catalog;
//   2. a catalog prefix ("Botón " -> "Button "), the rest being translated as a whole string when it is in the catalog;
//   3. a catalog suffix (" botones" -> " buttons"), the head being translated the same way.
// Anything else comes back unchanged.
std::string translate(Lang lang, std::string_view spanish);

// For the unit tests: how many whole-string entries there are, and the entry at an index.
struct CatalogEntry {
  const char* es;
  const char* en;
};
size_t catalogSize();
const CatalogEntry& catalogEntry(size_t index);

}  // namespace iris
