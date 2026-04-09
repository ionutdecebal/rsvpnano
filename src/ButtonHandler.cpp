#include "ButtonHandler.h"

void ButtonHandler::begin() {}

ButtonEvents ButtonHandler::update(bool rawPressed, bool pausedState, uint32_t nowMs) {
  ButtonEvents out{};

  if (rawPressed != lastRaw_) {
    lastBounceMs_ = nowMs;
    lastRaw_ = rawPressed;
  }

  if (nowMs - lastBounceMs_ >= BoardConfig::DEBOUNCE_MS) {
    stablePressed_ = rawPressed;
  }

  if (stablePressed_) {
    out.holdActive = true;
    wasHeld_ = true;
    releaseBufferedUntilMs_ = nowMs + BoardConfig::RELEASE_BUFFER_MS;
  } else {
    if (releaseBufferedUntilMs_ > nowMs) {
      out.holdActive = true;
    } else {
      if (wasHeld_) {
        out.pauseByRelease = true;
        wasHeld_ = false;
        if (pausedState) {
          if (clickCount_ == 0) {
            firstClickMs_ = nowMs;
          }
          clickCount_++;
        }
      }
    }
  }

  if (clickCount_ > 0 && (nowMs - firstClickMs_ > BoardConfig::TRIPLE_PRESS_WINDOW_MS)) {
    clickCount_ = 0;
  }

  if (pausedState && clickCount_ >= 3) {
    out.triplePress = true;
    clickCount_ = 0;
  }

  return out;
}
