#pragma once

#include <Arduino.h>
#include <algorithm>

namespace BoardDrivers::BatteryCurve {

inline uint8_t percentForVoltage(float voltage) {
  struct Point {
    float voltage;
    uint8_t percent;
  };

  constexpr Point kCurve[] = {
      {3.30f, 0},  {3.50f, 5},  {3.60f, 10}, {3.65f, 20},
      {3.70f, 30}, {3.75f, 40}, {3.79f, 50}, {3.85f, 60},
      {3.92f, 70}, {4.00f, 80}, {4.10f, 90}, {4.20f, 100},
  };

  if (voltage <= kCurve[0].voltage) {
    return kCurve[0].percent;
  }
  constexpr size_t curveSize = sizeof(kCurve) / sizeof(kCurve[0]);
  if (voltage >= kCurve[curveSize - 1].voltage) {
    return kCurve[curveSize - 1].percent;
  }

  for (size_t i = 1; i < curveSize; ++i) {
    const Point &upper = kCurve[i];
    const Point &lower = kCurve[i - 1];
    if (voltage > upper.voltage) {
      continue;
    }

    const float span = upper.voltage - lower.voltage;
    const float ratio = span <= 0.0f ? 0.0f : (voltage - lower.voltage) / span;
    const int percent =
        static_cast<int>(lower.percent + (upper.percent - lower.percent) * ratio + 0.5f);
    return static_cast<uint8_t>(std::max(0, std::min(100, percent)));
  }

  return 0;
}

}  // namespace BoardDrivers::BatteryCurve
