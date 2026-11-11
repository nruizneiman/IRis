// Start-up logic shared by the firmware and the simulator: load settings and the library, decide
// whether saving is safe, and keep a short history of boots (reset reasons) for diagnosis.
#pragma once

#include <string>
#include <vector>

#include "app/RemoteLibrary.h"
#include "app/Settings.h"
#include "diagnostics/RingLogSink.h"
#include "hardware/IBlobStore.h"
#include "persistence/LibraryRepository.h"

namespace iris {

struct BootReport {
  // False when the stored library could not be read for a reason that is NOT damage (I/O error, low
  // memory, data written by a newer firmware). The files are left untouched and saving is disabled so they
  // cannot be overwritten with an empty library.
  bool persistEnabled = true;
  std::string notice;  // Spanish text for a toast (translated when drawn, at most 21 columns), empty when all is fine
};

BootReport loadPersistentData(IBlobStore& store, LibraryRepository& repo, RemoteLibrary& library,
                              SettingsStore& settingsStore, Settings& settings, RingLogSink& log);

struct BootEntry {
  uint32_t number = 0;
  std::string reason;
};

// Last boots and why each one happened ("PANIC", "BROWNOUT", "WDT"...). Diagnostic only: damage to this
// file is tolerated (bad lines are skipped), never fatal.
class BootHistory {
 public:
  static constexpr size_t kMaxEntries = 12;
  static const char* key() { return "boots.log"; }

  explicit BootHistory(IBlobStore& store) : store_(store) {}
  std::vector<BootEntry> load() const;                 // oldest first
  Status record(const std::string& reason);            // appends one boot, keeps the newest kMaxEntries
  static std::vector<std::string> format(const std::vector<BootEntry>& entries);  // newest first, "#12 reason"

 private:
  IBlobStore& store_;
};

}  // namespace iris
