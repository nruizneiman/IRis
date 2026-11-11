// Rough battery level from the cell voltage. An ESTIMATE: it is shown with a "~" and never as a measurement.
#pragma once

#include <cstdint>
#include <optional>

namespace iris {

// Anchor points of a generic single-cell lithium-ion (18650) voltage / charge curve, taken from a vendor page
// (ufinebattery.com, "everything you should know about 18650 battery voltage"): 2.5 V = 0 %, 3.0 V = 10 %, 3.6 V = 50 %,
// 3.7 V = 60 %, 4.0 V = 90 %, 4.2 V = 100 %. Not measured on this cell. The page does not say whether the figures are at
// rest or under load, and under load the voltage reads lower, so the estimate tends to read low while the device runs.
//
// The top point is 4.10 V, chosen by the user on 2026-11-11 ("assume 4.10 V is 100 %"): this unit's charger left a cell that it
// called full at 4.11 V at rest, and a cell estimated at 4.10 V or more shows 100 %. That is a decision about what the
// display should call full, not a property of the cell: a lithium cell can be charged to 4.2 V.
// The voltage at which the boost converter stops is not established yet (test B9), so a low percentage does not mean
// the device still has that much time left.
struct BatteryCurvePoint {
  uint16_t millivolts;
  uint8_t percent;
};
inline constexpr BatteryCurvePoint kBatteryCurve[] = {{2500, 0}, {3000, 10}, {3600, 50}, {3700, 60}, {4000, 90}, {4100, 100}};
inline constexpr int kBatteryCurvePoints = static_cast<int>(sizeof kBatteryCurve / sizeof kBatteryCurve[0]);

// The divider reads the XL6009 input, not the cell: between the two the current of the running device crosses the charger's
// protection circuit, the power switch and the wiring. Measured on 2026-11-11 with the device on: cell 4.09 V, charger output
// 4.05 V, after the switch 4.00 V, divider 3.98 V, so 0.11 V in total (0.04 + 0.05 + 0.02). The drop follows the current (it is
// larger while transmitting, smaller when idle), so this fixed value is the one measured in normal operation, not an exact
// compensation. Remove it if the divider is ever moved to the cell terminals.
inline constexpr uint16_t kBatteryLoadDropMv = 110;

// The cell voltage estimated from what the divider reads.
inline uint16_t batteryCellMillivolts(uint16_t measuredMv) {
  const uint32_t cell = static_cast<uint32_t>(measuredMv) + kBatteryLoadDropMv;
  return static_cast<uint16_t>(cell > 65535u ? 65535u : cell);
}

// Linear interpolation between the anchor points; clamped to 0..100.
inline int batteryPercentApprox(uint16_t mv) {
  if (mv <= kBatteryCurve[0].millivolts) return 0;
  if (mv >= kBatteryCurve[kBatteryCurvePoints - 1].millivolts) return 100;
  for (int i = 1; i < kBatteryCurvePoints; ++i) {
    const BatteryCurvePoint lo = kBatteryCurve[i - 1], hi = kBatteryCurve[i];
    if (mv <= hi.millivolts) {
      const int span = hi.millivolts - lo.millivolts;
      return lo.percent + (hi.percent - lo.percent) * (mv - lo.millivolts) / span;
    }
  }
  return 100;
}

// 0..4 bars: 4 from 75 %, 3 from 50 %, 2 from 25 %, 1 from 10 %, otherwise an empty outline.
inline int batterySegments(int percent) {
  if (percent >= 75) return 4;
  if (percent >= 50) return 3;
  if (percent >= 25) return 2;
  if (percent >= 10) return 1;
  return 0;
}

// What the screen shows: the voltage is noisy (ADC) and dips with every transmission, so a raw estimate would flicker.
// update() takes at most one sample per kSampleIntervalMs, smooths it (exponential average, 1/8 of the difference per
// sample) and moves the shown percentage only when the new value differs from it by kHysteresisPercent or more.
// A missing reading (no circuit) clears it, and the next reading starts fresh.
class BatteryEstimator {
 public:
  static constexpr uint32_t kSampleIntervalMs = 500;
  static constexpr int kHysteresisPercent = 2;

  void update(uint32_t nowMs, std::optional<uint16_t> mv) {
    if (!mv) {
      have_ = false;
      return;
    }
    if (have_ && nowMs - lastMs_ < kSampleIntervalMs) return;
    lastMs_ = nowMs;
    const int32_t sample = static_cast<int32_t>(*mv) * kScale;
    smoothed_ = have_ ? smoothed_ + (sample - smoothed_) / 8 : sample;
    const int now = batteryPercentApprox(static_cast<uint16_t>(smoothed_ / kScale));
    if (!have_ || now - shown_ >= kHysteresisPercent || shown_ - now >= kHysteresisPercent) shown_ = now;
    have_ = true;
  }

  bool valid() const { return have_; }
  int percent() const { return shown_; }  // meaningful only when valid()
  int segments() const { return batterySegments(shown_); }

 private:
  static constexpr int32_t kScale = 16;  // fixed point: keeps the 1/8 step from losing the small differences
  bool have_ = false;
  uint32_t lastMs_ = 0;
  int32_t smoothed_ = 0;
  int shown_ = 0;
};

}  // namespace iris
