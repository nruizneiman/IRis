#include <doctest.h>

#include "app/BatteryLevel.h"

using namespace iris;

TEST_CASE("battery level: the anchor points of the generic curve map to their percentages") {
  CHECK(batteryPercentApprox(4200) == 100);
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
