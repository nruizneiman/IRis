#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#include <map>
#include <string>
#include <vector>

#include "hardware/IBlobStore.h"

// Flash-backed blob store on LittleFS with the atomic-write contract of IBlobStore:
//   write -> "<key>.tmp" (fully written, size verified) -> rename over "<key>".
// A power cut leaves either the old file or the new one; leftover ".tmp" files are removed at mount.
// ASSUMPTION TO VERIFY ON HARDWARE: rename() onto an existing file is atomic in this LittleFS port
// (documented behaviour of littlefs; run the power-cut test in docs/hardware/bring-up.md).
//
// If the partition cannot be mounted (never formatted, or damaged) the store does NOT format by
// itself: it falls back to RAM so the device stays usable, reports degraded(), and the user can
// choose to format from Configuración > Almacenamiento. Data is never erased silently.
class PersistentStore : public iris::IBlobStore {
 public:
  static constexpr size_t kFreeMargin = 1024;  // keep some room for littlefs metadata

  // Mounts without formatting. Always leaves the store usable (flash or RAM fallback).
  iris::Status begin() {
    mounted_ = LittleFS.begin(/*formatOnFail=*/false);
    if (!mounted_) {
      return iris::Status::fail(iris::Err::Corrupt, "LittleFS mount failed (unformatted or damaged)");
    }
    removeStaleTemps();
    return iris::Status::success();
  }

  // User-confirmed: erases the whole partition, then mounts the fresh file system.
  iris::Status format() {
    if (mounted_) LittleFS.end();
    if (!LittleFS.format()) return iris::Status::fail(iris::Err::StorageFailure, "format failed");
    mounted_ = LittleFS.begin(false);
    if (!mounted_) return iris::Status::fail(iris::Err::StorageFailure, "mount after format failed");
    ram_.clear();
    return iris::Status::success();
  }

  bool degraded() const { return !mounted_; }
  size_t totalBytes() const { return mounted_ ? LittleFS.totalBytes() : 0; }
  size_t usedBytes() const { return mounted_ ? LittleFS.usedBytes() : 0; }

  bool exists(const std::string& key) const override {
    if (!validKey(key)) return false;
    return mounted_ ? LittleFS.exists(path(key).c_str()) : ram_.count(key) != 0;
  }

  iris::Result<std::string> read(const std::string& key) const override {
    if (!validKey(key)) return iris::Status::fail(iris::Err::InvalidArgument, "bad key");
    if (!mounted_) {
      auto it = ram_.find(key);
      if (it == ram_.end()) return iris::Status::fail(iris::Err::NotFound, "no such key");
      return it->second;
    }
    File f = LittleFS.open(path(key).c_str(), "r");
    if (!f) return iris::Status::fail(iris::Err::NotFound, "no such key");
    std::string out;
    out.resize(f.size());
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&out[0]), out.size());
    f.close();
    if (n != out.size()) return iris::Status::fail(iris::Err::StorageFailure, "short read");
    return out;
  }

  iris::Status write(const std::string& key, const std::string& data) override {
    if (!validKey(key)) return iris::Status::fail(iris::Err::InvalidArgument, "bad key");
    if (!mounted_) {
      ram_[key] = data;
      return iris::Status::success();
    }
    if (totalBytes() - usedBytes() < data.size() + kFreeMargin) {
      return iris::Status::fail(iris::Err::StorageFailure, "flash full");
    }
    const std::string tmp = path(key) + ".tmp";
    {
      File f = LittleFS.open(tmp.c_str(), "w");
      if (!f) return iris::Status::fail(iris::Err::StorageFailure, "cannot create temp file");
      const size_t n = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
      f.flush();
      f.close();
      if (n != data.size()) {
        LittleFS.remove(tmp.c_str());
        return iris::Status::fail(iris::Err::StorageFailure, "short write");
      }
    }
    {
      File f = LittleFS.open(tmp.c_str(), "r");  // verify the size that actually reached the flash
      const bool ok = f && f.size() == data.size();
      if (f) f.close();
      if (!ok) {
        LittleFS.remove(tmp.c_str());
        return iris::Status::fail(iris::Err::StorageFailure, "verification failed");
      }
    }
    if (!LittleFS.rename(tmp.c_str(), path(key).c_str())) {
      LittleFS.remove(tmp.c_str());
      return iris::Status::fail(iris::Err::StorageFailure, "rename failed");
    }
    return iris::Status::success();
  }

  iris::Status remove(const std::string& key) override {
    if (!validKey(key)) return iris::Status::fail(iris::Err::InvalidArgument, "bad key");
    if (!mounted_) return ram_.erase(key) ? iris::Status::success() : iris::Status::fail(iris::Err::NotFound, "no such key");
    return LittleFS.remove(path(key).c_str()) ? iris::Status::success() : iris::Status::fail(iris::Err::NotFound, "no such key");
  }

 std::vector<std::string> list() const override {
    std::vector<std::string> keys;
    if (!mounted_) {
      for (const auto& kv : ram_) keys.push_back(kv.first);
      return keys;
    }
    File root = LittleFS.open("/");
    if (!root) return keys;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
      std::string name = f.name();
      if (!name.empty() && name[0] == '/') name.erase(0, 1);
      if (validKey(name)) keys.push_back(name);
    }
    root.close();
    return keys;
  }

 private:
  // Lowercase letters, digits, '.', '-', '_' ; short; never a ".tmp" name (reserved for atomic writes).
  static bool validKey(const std::string& k) {
    if (k.empty() || k.size() > 24) return false;
    if (k.size() >= 4 && k.compare(k.size() - 4, 4, ".tmp") == 0) return false;
    for (char c : k) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
      if (!ok) return false;
    }
    return true;
  }
  static std::string path(const std::string& key) { return "/" + key; }

  void removeStaleTemps() {
    File root = LittleFS.open("/");
    if (!root) return;
    std::string stale[8];
    size_t count = 0;
    for (File f = root.openNextFile(); f && count < 8; f = root.openNextFile()) {
      std::string name = f.name();
      if (!name.empty() && name[0] == '/') name.erase(0, 1);  // some cores return the full path
      if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".tmp") == 0) stale[count++] = name;
    }
    root.close();
    for (size_t i = 0; i < count; ++i) LittleFS.remove(("/" + stale[i]).c_str());
  }

  bool mounted_ = false;
  std::map<std::string, std::string> ram_;  // fallback while the flash is not mounted
};
