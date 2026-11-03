// Rough battery level from the cell voltage. An ESTIMATE: it is shown with a "~" and never as a measurement.
#pragma once

#include <cstdint>

namespace iris {

// Anchor points of a generic single-cell lithium-ion (18650) voltage / charge curve, taken from a vendor page
// (ufinebattery.com, "everything you should know about 18650 battery voltage"): 2.5 V = 0 %, 3.0 V = 10 %, 3.6 V = 50 %,
// 3.7 V = 60 %, 4.0 V = 90 %, 4.2 V = 100 %. Not measured on this cell. The page does not say whether the figures are at
// rest or under load, and under load the voltage reads lower, so the estimate tends to read low while the device runs.
// The voltage at which the boost converter stops is not established yet (test B9), so a low percentage does not mean
// the device still has that much time left.
struct BatteryCurvePoint {
  uint16_t millivolts;
  uint8_t percent;
};
inline constexpr BatteryCurvePoint kBatteryCurve[] = {{2500, 0}, {3000, 10}, {3600, 50}, {3700, 60}, {4000, 90}, {4200, 100}};
inline constexpr int kBatteryCurvePoints = static_cast<int>(sizeof kBatteryCurve / sizeof kBatteryCurve[0]);

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

}  // namespace iris
