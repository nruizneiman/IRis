#pragma once

#include <cstdint>
#include <optional>

namespace iris {

// std::nullopt means "no measurement circuit": the UI must say "not available", never invent a %.
class IBatteryMonitor {
 public:
  virtual ~IBatteryMonitor() = default;
  virtual std::optional<uint16_t> readMillivolts() = 0;
};

}  // namespace iris
