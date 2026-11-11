#include "app/Settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "persistence/Crc32.h"

namespace iris {

std::string serializeSettings(const Settings& s) {
  char body[80];
  std::snprintf(body, sizeof body, "invert=%d\ntimeout=%u\nlang=%u\n", s.invertDisplay ? 1 : 0,
                static_cast<unsigned>(s.captureTimeoutSec), static_cast<unsigned>(s.language));
  const std::string b = body;
  char header[48];
  std::snprintf(header, sizeof header, "IRISCFG 1 %08lx %lu\n", static_cast<unsigned long>(crc32(b)),
                static_cast<unsigned long>(b.size()));
  return std::string(header) + b;
}

Result<Settings> deserializeSettings(const std::string& text) {
  const size_t nl = text.find('\n');
  if (nl == std::string::npos || nl > 40) return Status::fail(Err::Corrupt, "missing header");
  unsigned version = 0;
  unsigned long crc = 0, length = 0;
  char magic[12] = {0};
  if (std::sscanf(text.substr(0, nl).c_str(), "%11s %u %lx %lu", magic, &version, &crc, &length) != 4 ||
      std::strcmp(magic, "IRISCFG") != 0) {
    return Status::fail(Err::Corrupt, "bad header");
  }
  if (version != 1) return Status::fail(Err::UnsupportedVersion, "unknown settings version");
  const std::string body = text.substr(nl + 1);
  if (body.size() != length || crc32(body) != static_cast<uint32_t>(crc)) {
    return Status::fail(Err::Corrupt, "checksum or length mismatch");
  }
  Settings s;
  size_t pos = 0;
  while (pos < body.size()) {
    size_t end = body.find('\n', pos);
    if (end == std::string::npos) end = body.size();
    const std::string line = body.substr(pos, end - pos);
    pos = end + 1;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq);
    const long v = std::strtol(line.c_str() + eq + 1, nullptr, 10);
    if (k == "invert" && (v == 0 || v == 1)) s.invertDisplay = v == 1;
    else if (k == "timeout" && v > 0 && Settings::validTimeout(static_cast<unsigned>(v))) {
      s.captureTimeoutSec = static_cast<uint8_t>(v);
    } else if (k == "lang" && v >= 0 && v < static_cast<long>(kLangCount)) {
      s.language = static_cast<Lang>(v);
    }  // other keys (for example "card" of an earlier version) are ignored
  }
  return s;
}

Settings SettingsStore::load(Status* status) const {
  if (status) *status = Status::success();
  if (!store_.exists(key())) return Settings{};
  Result<std::string> raw = store_.read(key());
  Result<Settings> parsed = raw.ok() ? deserializeSettings(raw.value()) : Result<Settings>(raw.status());
  if (parsed.ok()) return parsed.value();
  if (status) *status = parsed.status();
  return Settings{};
}

Status SettingsStore::save(const Settings& s) { return store_.write(key(), serializeSettings(s)); }

}  // namespace iris
