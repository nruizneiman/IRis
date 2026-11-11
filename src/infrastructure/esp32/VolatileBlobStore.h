#pragma once

#include <map>
#include <string>
#include <vector>

#include "hardware/IBlobStore.h"

// RAM-only store from before the LittleFS store existed, not used by the current firmware: data is LOST on reset.
class VolatileBlobStore : public iris::IBlobStore {
 public:
  bool exists(const std::string& key) const override { return files_.count(key) != 0; }
  iris::Result<std::string> read(const std::string& key) const override {
    auto it = files_.find(key);
    if (it == files_.end()) return iris::Status::fail(iris::Err::NotFound, "no such key");
    return it->second;
  }
  iris::Status write(const std::string& key, const std::string& data) override {
    files_[key] = data;
    return iris::Status::success();
  }
  iris::Status remove(const std::string& key) override {
    return files_.erase(key) ? iris::Status::success() : iris::Status::fail(iris::Err::NotFound, "no such key");
  }

 std::vector<std::string> list() const override {
    std::vector<std::string> keys;
    for (const auto& kv : files_) keys.push_back(kv.first);
    return keys;
  }

 private:
  std::map<std::string, std::string> files_;
};
