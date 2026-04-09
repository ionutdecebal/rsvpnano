#include "TouchHandler.h"

void TouchHandler::reset() {
  tracking_ = false;
  longPressFired_ = false;
}

GestureResult TouchHandler::update(const TouchEvent& e, bool touchEnabled, uint32_t nowMs) {
  GestureResult g{};
  if (!touchEnabled || !e.valid) {
    return g;
  }

  if (e.pressed && !tracking_) {
    tracking_ = true;
    longPressFired_ = false;
    startX_ = lastX_ = e.x;
    startY_ = lastY_ = e.y;
    startMs_ = lastMs_ = nowMs;
    return g;
  }

  if (!e.pressed && tracking_) {
    tracking_ = false;
    return g;
  }

  if (!tracking_) {
    return g;
  }

  const int dx = e.x - lastX_;
  const int dy = e.y - lastY_;
  const uint32_t dt = max<uint32_t>(1, nowMs - lastMs_);

  const int totalDx = e.x - startX_;
  const int totalDy = e.y - startY_;

  if (!longPressFired_ && abs(totalDx) < 10 && abs(totalDy) < 10 && (nowMs - startMs_ >= BoardConfig::LONG_PRESS_MS)) {
    g.longPress = true;
    longPressFired_ = true;
  }

  const float vx = (float)dx / (float)dt;
  const float vy = (float)dy / (float)dt;

  if (abs(totalDx) > abs(totalDy) && abs(dx) >= 2) {
    g.horizontalScrub = true;
    int magnitude = abs(dx) / 6 + abs((int)(vx * 20.0f));
    g.scrubDelta = (dx > 0 ? 1 : -1) * max(1, magnitude);
  } else if (abs(totalDy) > abs(totalDx) && abs(dy) >= 3) {
    g.verticalWpm = true;
    int steps = max(1, abs(dy) / 18 + abs((int)(vy * 10.0f)));
    g.wpmDelta = (dy < 0 ? 1 : -1) * steps * 10;
  }

  lastX_ = e.x;
  lastY_ = e.y;
  lastMs_ = nowMs;
  return g;
}
