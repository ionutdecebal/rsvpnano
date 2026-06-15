#include "input/InputButtons.h"

namespace Input::Buttons {

Button::Button(int pin) : pin_(pin) {}

void Button::resetState(bool held, uint32_t nowMs) {
  held_ = held;
  pressedEvent_ = false;
  releasedEvent_ = false;
  lastEdgeMs_ = nowMs;
  pressStartedMs_ = held ? nowMs : 0;
  lastHoldDurationMs_ = 0;
}

void Button::begin() {
  if (pin_ < 0) {
    resetState(false, millis());
    return;
  }

  pinMode(pin_, INPUT_PULLUP);
  resetState(!digitalRead(pin_), millis());
}

void Button::beginWithState(bool held) { resetState(held, millis()); }

void Button::updateFromState(bool currentHeld, uint32_t nowMs) {
  pressedEvent_ = false;
  releasedEvent_ = false;

  if (currentHeld != held_) {
    held_ = currentHeld;
    lastEdgeMs_ = nowMs;
    if (held_) {
      pressStartedMs_ = nowMs;
      pressedEvent_ = true;
    } else {
      lastHoldDurationMs_ = nowMs - pressStartedMs_;
      releasedEvent_ = true;
    }
  }
}

void Button::update(uint32_t nowMs) {
  if (pin_ < 0) {
    pressedEvent_ = false;
    releasedEvent_ = false;
    return;
  }

  updateFromState(!digitalRead(pin_), nowMs);  // Board buttons are active-low.
}

bool Button::isHeld() const { return held_; }

bool Button::wasPressedEvent() const { return pressedEvent_; }

bool Button::wasReleasedEvent() const { return releasedEvent_; }

uint32_t Button::lastEdgeMs() const { return lastEdgeMs_; }

uint32_t Button::heldDurationMs(uint32_t nowMs) const {
  return held_ ? nowMs - pressStartedMs_ : 0;
}

uint32_t Button::lastHoldDurationMs() const { return lastHoldDurationMs_; }

}  // namespace Input::Buttons
