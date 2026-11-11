#include "app/Boot.h"

#include <cstdio>
#include <cstdlib>

namespace iris {

BootReport loadPersistentData(IBlobStore& store, LibraryRepository& repo, RemoteLibrary& library,
                              SettingsStore& settingsStore, Settings& settings, RingLogSink& log) {
  BootReport report;

  Status settingsStatus;
  settings = settingsStore.load(&settingsStatus);
  if (!settingsStatus.ok()) log.log(LogLevel::Warn, "boot", "settings not loaded: " + settingsStatus.message);

  Result<LoadOutcome> loaded = repo.load();
  if (loaded.ok()) {
    LoadOutcome& out = loaded.value();
    library = std::move(out.library);
    if (out.source == LoadSource::Backup) {
      log.log(LogLevel::Warn, "boot", "library restored from backup index: " + out.primaryStatus.message);
      report.notice = "Se usó el respaldo";
    }
    if (out.source == LoadSource::Legacy) log.log(LogLevel::Info, "boot", "format-1 library, migrates on next save");
    if (!out.lostRemotes.empty()) {
      for (const std::string& name : out.lostRemotes) log.log(LogLevel::Error, "boot", "remote unreadable: " + name);
      report.notice = out.lostRemotes.size() == 1 ? "Perdido: " + out.lostRemotes[0]
                                                   : std::to_string(out.lostRemotes.size()) + " controles perdidos";
    }
    return report;
  }

  const Status& st = loaded.status();
  log.log(LogLevel::Error, "boot", "library not loaded: " + st.message);
  if (st.code == Err::Corrupt) {
    // Damaged beyond use: start empty, but keep the unreadable index for inspection before it is replaced.
    auto raw = store.read(LibraryRepository::indexKey());
    if (raw.ok()) store.write("badidx.irs", raw.value());
    report.notice = "Datos dañados: vacío";
    return report;
  }
  // Transient trouble or newer data: do not let a later save replace what we could not read.
  report.persistEnabled = false;
  report.notice = st.code == Err::UnsupportedVersion ? "Datos de otra versión" : "No leído: sin guardar";
  return report;
}

std::vector<BootEntry> BootHistory::load() const {
  std::vector<BootEntry> out;
  Result<std::string> raw = store_.read(key());
  if (!raw.ok()) return out;
  // Manual line splitting: std::istringstream would pull the whole iostream/locale machinery into the firmware.
  const std::string& text = raw.value();
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find(char(10), start);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(start, end - start);
    start = end + 1;
    const size_t sp = line.find(' ');
    if (sp == std::string::npos || sp == 0) continue;
    char* stop = nullptr;
    const unsigned long n = std::strtoul(line.c_str(), &stop, 10);
    if (stop != line.c_str() + sp) continue;  // not "<number> <reason>"
    out.push_back(BootEntry{static_cast<uint32_t>(n), line.substr(sp + 1)});
  }
  if (out.size() > kMaxEntries) out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(kMaxEntries));
  return out;
}

Status BootHistory::record(const std::string& reason) {
  std::vector<BootEntry> entries = load();
  const uint32_t next = entries.empty() ? 1 : entries.back().number + 1;
  std::string clean = reason.substr(0, 24);
  for (char& c : clean) {
    if (c == '\n' || c == '\r') c = ' ';
  }
  entries.push_back(BootEntry{next, clean});
  if (entries.size() > kMaxEntries) entries.erase(entries.begin(), entries.end() - static_cast<std::ptrdiff_t>(kMaxEntries));
  std::string text;
  for (const BootEntry& e : entries) text += std::to_string(e.number) + " " + e.reason + std::string(1, 10);
  return store_.write(key(), text);
}

std::vector<std::string> BootHistory::format(const std::vector<BootEntry>& entries) {
  std::vector<std::string> out;
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) out.push_back("#" + std::to_string(it->number) + " " + it->reason);
  return out;
}

}  // namespace iris
