#include <doctest.h>

#include "app/BatteryLevel.h"

using namespace iris;

TEST_CASE("battery level: the anchor points of the generic curve map to their percentages") {
  CHECK(batteryPercentApprox(4200) == 100);
  CHECK(batteryPercentApprox(4100) == 100);  // what this unit's display calls full
  CHECK(batteryPercentApprox(4050) == 95);
  CHECK(batteryPercentApprox(4000) == 90);
  CHECK(batteryPercentApprox(3700) == 60);
  CHECK(batteryPercentApprox(3600) == 50);
  CHECK(batteryPercentApprox(3000) == 10);
  CHECK(batteryPercentApprox(2500) == 0);
}

TEST_CASE("battery level: values between anchors are interpolated and the range is clamped") {
  CHECK(batteryPercentApprox(3800) == 70);   // halfway between 3.7 V (60 %) and 4.0 V (90 %): 60 + 30 * 100 / 300
  CHECK(batteryPercentApprox(3300) == 30);   // halfway between 3.0 V (10 %) and 3.6 V (50 %)
  CHECK(batteryPercentApprox(5000) == 100);
  CHECK(batteryPercentApprox(0) == 0);
  CHECK(batteryPercentApprox(2000) == 0);
}

TEST_CASE("battery level: the estimate never decreases when the voltage rises") {
  int previous = 0;
  for (int mv = 2000; mv <= 4400; mv += 10) {
    const int p = batteryPercentApprox(static_cast<uint16_t>(mv));
    CHECK(p >= previous);
    previous = p;
  }
}

TEST_CASE("battery level: bars follow the percentage thresholds") {
  CHECK(batterySegments(100) == 4);
  CHECK(batterySegments(75) == 4);
  CHECK(batterySegments(74) == 3);
  CHECK(batterySegments(50) == 3);
  CHECK(batterySegments(49) == 2);
  CHECK(batterySegments(25) == 2);
  CHECK(batterySegments(24) == 1);
  CHECK(batterySegments(10) == 1);
  CHECK(batterySegments(9) == 0);
  CHECK(batterySegments(0) == 0);
}

TEST_CASE("battery estimator: sampling noise does not move the shown percentage") {
  BatteryEstimator e;
  uint32_t t = 0;
  e.update(t, 3770);
  const int first = e.percent();
  int changes = 0;
  for (int i = 0; i < 200; ++i) {
    t += 500;
    const int before = e.percent();
    e.update(t, (i % 2) ? 3800 : 3740);  // +-30 mV around 3.77 V
    if (e.percent() != before) ++changes;
  }
  CHECK(e.valid());
  CHECK(changes <= 2);
  CHECK(e.percent() >= first - 3);
  CHECK(e.percent() <= first + 3);
}

TEST_CASE("battery estimator: one sample per interval, and a real drop is followed") {
  BatteryEstimator e;
  e.update(0, 4000);
  CHECK(e.percent() == 90);
  e.update(100, 3000);  // too soon: ignored
  CHECK(e.percent() == 90);
  uint32_t t = 0;
  for (int i = 0; i < 60; ++i) e.update(t += BatteryEstimator::kSampleIntervalMs, 3600);
  CHECK(e.percent() == 50);
}

TEST_CASE("battery estimator: a missing reading clears it and the next one starts fresh") {
  BatteryEstimator e;
  e.update(0, 4000);
  REQUIRE(e.valid());
  e.update(600, std::nullopt);
  CHECK_FALSE(e.valid());
  e.update(700, 3600);  // starts at the new value, not an average with the old one
  CHECK(e.valid());
  CHECK(e.percent() == 50);
}

TEST_CASE("battery level: the divider reading is raised by the measured drop to estimate the cell") {
  CHECK(batteryCellMillivolts(3980) == 4090);   // the 2026-11-11 measurement: divider 3.98 V, cell 4.09 V under load
  CHECK(batteryCellMillivolts(65500) == 65535);  // saturates instead of wrapping
  CHECK(batteryPercentApprox(batteryCellMillivolts(3980)) == 99);
}
