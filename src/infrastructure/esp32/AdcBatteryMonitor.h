#pragma once

#include <Arduino.h>

#include <algorithm>
#include <optional>

#include "board_pins.h"
#include "hardware/IBatteryMonitor.h"

// Battery voltage from a resistor divider on board::kBatterySense (GPIO33, ADC1). Compiled in only when the build defines
// IRIS_BATTERY_RATIO_PERMILLE (divider ratio x 1000, e.g. 2000 for two equal resistors): with the pin unconnected the ADC
// reads noise, and the UI must say "not available" instead of showing it.
//
// Not verified on the board. analogReadMilliVolts() applies the chip's factory calibration (typically within a few
// percent); compare the value with a multimeter on the battery before trusting it, and adjust the ratio if the two differ.
class AdcBatteryMonitor : public iris::IBatteryMonitor {
 public:
  void begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(board::kBatterySense, ADC_11db);  // about 0 to 3.1 V at the pin
  }

  std::optional<uint16_t> readMillivolts() override {
    uint32_t samples[kSamples];
    for (uint32_t& s : samples) {
      s = analogReadMilliVolts(board::kBatterySense);
      delayMicroseconds(200);
    }
    std::sort(samples, samples + kSamples);
    const uint32_t atPin = samples[kSamples / 2];  // median: ignores a stray spike
    if (atPin < kMinPinMv) return std::nullopt;     // nothing connected / battery rail off
    const uint32_t battery = atPin * kRatioPermille / 1000u;
    return static_cast<uint16_t>(std::min<uint32_t>(battery, 65535u));
  }

 private:
  static constexpr int kSamples = 9;
  static constexpr uint32_t kMinPinMv = 300;  // under this the divider is not powered (a 2.4 V cell would give 1.2 V)
  static constexpr uint32_t kRatioPermille = IRIS_BATTERY_RATIO_PERMILLE;
};
