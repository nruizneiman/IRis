// Small user preferences, persisted as a checksummed text blob ("IRISCFG 1 <crc> <len>\nkey=value...").
// Unknown keys are ignored; out-of-range values fall back to defaults; a corrupt file yields defaults.
#pragma once

#include <cstdint>

#include "Result.h"
#include "domain/Lang.h"
#include "hardware/IBlobStore.h"

namespace iris {

struct Settings {
  bool invertDisplay = false;
  uint8_t captureTimeoutSec = 15;  // one of 5, 10, 15, 30
  Lang language = kDefaultLang;
  bool cardInLibrary = true;  // list the card's folders next to the saved categories in Controles

  static bool validTimeout(unsigned s) { return s == 5 || s == 10 || s == 15 || s == 30; }
};

class SettingsStore {
 public:
  explicit SettingsStore(IBlobStore& store) : store_(store) {}
  // Always returns usable settings; `status` (if given) says why defaults were used.
  Settings load(Status* status = nullptr) const;
  Status save(const Settings& settings);
  static const char* key() { return "settings.cfg"; }

 private:
  IBlobStore& store_;
};

std::string serializeSettings(const Settings& s);
Result<Settings> deserializeSettings(const std::string& text);

}  // namespace iris
