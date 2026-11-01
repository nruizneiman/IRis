#pragma once

#include <Arduino.h>

#include "hardware/IClock.h"

class ArduinoClock : public iris::IClock {
 public:
  uint32_t nowMs() const override { return millis(); }
  uint32_t nowUs() const override { return micros(); }
};
