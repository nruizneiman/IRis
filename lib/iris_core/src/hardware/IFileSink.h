#pragma once

#include <string>

#include "Result.h"

namespace iris {

// Writing side of the removable card. Used only when the user asks to export; nothing is written otherwise.
class IFileSink {
 public:
  virtual ~IFileSink() = default;
  virtual bool writable() = 0;  // a card is present and mounted
  // Writes the whole file, creating the folders on its path. An existing file is replaced, and a failure part-way leaves
  // the old file in place (the adapter writes to a temporary name first).
  virtual Status writeText(const std::string& path, const std::string& text) = 0;
};

}  // namespace iris
