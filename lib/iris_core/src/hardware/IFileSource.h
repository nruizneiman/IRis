#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Result.h"

namespace iris {

struct FileEntry {
  std::string name;  // last path component, no directory part
  bool isDir = false;
  uint32_t size = 0;
};

// Read-only view of a removable card (the Flipper-IRDB files). `available()` is false when there is no card or it could
// not be mounted; the app then keeps working with its internal storage only.
class IFileSource {
 public:
  virtual ~IFileSource() = default;
  virtual bool available() = 0;
  // Entries directly inside `dir` (any order), at most `maxEntries`.
  virtual Result<std::vector<FileEntry>> list(const std::string& dir, size_t maxEntries) = 0;
  // Whole file as text; fails with OutOfRange when it is larger than `maxBytes`.
  virtual Result<std::string> readText(const std::string& path, size_t maxBytes) = 0;
};

}  // namespace iris
