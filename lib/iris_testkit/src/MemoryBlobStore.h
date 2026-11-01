#pragma once

#include <map>
#include <string>
#include <vector>

#include "hardware/IBlobStore.h"

namespace iris::testkit {

// In-memory IBlobStore with fault injection. write() is atomic, as the interface requires.
class MemoryBlobStore : public IBlobStore {
 public:
  bool exists(const std::string& key) const override { return files_.count(key) != 0; }

  Result<std::string> read(const std::string& key) const override {
    if (failReads_) return Status::fail(Err::StorageFailure, "injected read failure");
    auto it = files_.find(key);
    if (it == files_.end()) return Status::fail(Err::NotFound, "no such key");
    return it->second;
  }

  Status write(const std::string& key, const std::string& data) override {
    if (writesUntilFailure_ == 0) return Status::fail(Err::StorageFailure, "injected write failure");
    if (writesUntilFailure_ > 0) --writesUntilFailure_;
    files_[key] = data;
    return Status::success();
  }

  Status remove(const std::string& key) override {
    return files_.erase(key) ? Status::success() : Status::fail(Err::NotFound, "no such key");
  }

  std::vector<std::string> list() const override {
    std::vector<std::string> keys;
    for (const auto& kv : files_) keys.push_back(kv.first);
    return keys;
  }

  // Fault injection ---------------------------------------------------------
  // The next `n` writes succeed, every later one fails. n < 0 disables injection.
  void failWritesAfter(int n) { writesUntilFailure_ = n; }
  void setFailReads(bool fail) { failReads_ = fail; }
  void flipByte(const std::string& key, size_t offset) { files_.at(key).at(offset) ^= 0x01; }
  void truncate(const std::string& key, size_t newSize) { files_.at(key).resize(newSize); }
  void put(const std::string& key, const std::string& data) { files_[key] = data; }

 private:
  std::map<std::string, std::string> files_;
  int writesUntilFailure_ = -1;
  bool failReads_ = false;
};

}  // namespace iris::testkit
