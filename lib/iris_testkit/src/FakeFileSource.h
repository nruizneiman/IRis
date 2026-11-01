#pragma once

#include <map>
#include <set>
#include <string>

#include "hardware/IFileSource.h"

namespace iris::testkit {

// In-memory card: paths map to contents; folders exist implicitly.
class FakeFileSource : public IFileSource {
 public:
  bool available() override { return present_; }
  void setPresent(bool p) { present_ = p; }
  void add(const std::string& path, const std::string& content) { files_[path] = content; }

  Result<std::vector<FileEntry>> list(const std::string& dir, size_t maxEntries) override {
    if (!present_) return Status::fail(Err::Unavailable, "no card");
    const std::string prefix = dir.empty() || dir.back() != '/' ? dir + "/" : dir;
    std::vector<FileEntry> out;
    std::set<std::string> seenDirs;
    for (const auto& kv : files_) {
      if (kv.first.compare(0, prefix.size(), prefix) != 0) continue;
      const std::string rest = kv.first.substr(prefix.size());
      const size_t slash = rest.find('/');
      if (slash == std::string::npos) {
        out.push_back(FileEntry{rest, false, static_cast<uint32_t>(kv.second.size())});
      } else if (seenDirs.insert(rest.substr(0, slash)).second) {
        out.push_back(FileEntry{rest.substr(0, slash), true, 0});
      }
      if (out.size() >= maxEntries) break;
    }
    return out;
  }

  Result<std::string> readText(const std::string& path, size_t maxBytes) override {
    if (!present_) return Status::fail(Err::Unavailable, "no card");
    auto it = files_.find(path);
    if (it == files_.end()) return Status::fail(Err::NotFound, "no such file");
    if (it->second.size() > maxBytes) return Status::fail(Err::OutOfRange, "file too large");
    return it->second;
  }

 private:
  std::map<std::string, std::string> files_;
  bool present_ = true;
};

}  // namespace iris::testkit
