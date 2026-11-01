#pragma once

#include <cstdint>

namespace iris {

// Monotonic time source. 32-bit values wrap; compare with subtraction (now - start).
class IClock {
 public:
  virtual ~IClock() = default;
  virtual uint32_t nowMs() const = 0;
  virtual uint32_t nowUs() const = 0;
};

}  // namespace iris
