#pragma once

#include <string>
#include <vector>

#include "Result.h"

namespace iris {

// Named byte blobs. Contract: write() is atomic - after a power loss a reader sees either the
// complete old content or the complete new content, never a mix (LittleFS: temp file + rename).
class IBlobStore {
 public:
  virtual ~IBlobStore() = default;
  virtual bool exists(const std::string& key) const = 0;
  virtual Result<std::string> read(const std::string& key) const = 0;
  virtual Status write(const std::string& key, const std::string& data) = 0;
  virtual Status remove(const std::string& key) = 0;
  // Keys currently stored (any order). Used to find orphaned files after an interrupted save.
  virtual std::vector<std::string> list() const = 0;
};

}  // namespace iris
