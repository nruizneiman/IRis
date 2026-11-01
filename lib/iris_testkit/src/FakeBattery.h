#pragma once

#include "hardware/IBatteryMonitor.h"

namespace iris::testkit {

class FakeBattery : public IBatteryMonitor {
 public:
  std::optional<uint16_t> readMillivolts() override { return mv_; }
  void set(std::optional<uint16_t> mv) { mv_ = mv; }  // nullopt = no measurement circuit

 private:
  std::optional<uint16_t> mv_;
};

}  // namespace iris::testkit
