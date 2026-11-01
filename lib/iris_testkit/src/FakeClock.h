// Host-only test doubles. Never linked into the firmware.
#pragma once

#include "hardware/IClock.h"

namespace iris::testkit {

class FakeClock : public IClock {
 public:
  uint32_t nowMs() const override { return static_cast<uint32_t>(us_ / 1000u); }
  uint32_t nowUs() const override { return static_cast<uint32_t>(us_); }
  void advanceMs(uint32_t ms) { us_ += static_cast<uint64_t>(ms) * 1000u; }
  void advanceUs(uint32_t us) { us_ += us; }
  void setUs(uint64_t us) { us_ = us; }  // lets tests cross the 32-bit wrap point

 private:
  uint64_t us_ = 0;
};

}  // namespace iris::testkit
