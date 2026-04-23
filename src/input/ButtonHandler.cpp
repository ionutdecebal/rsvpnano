#include "input/ButtonHandler.h"

ButtonHandler::ButtonHandler(int pin) : pin_(pin) {}

void ButtonHandler::begin() {
  pinMode(pin_, INPUT_PULLUP);
  held_ = !digitalRead(pin_);
  pressedEvent_ = false;
  releasedEvent_ = false;
  lastEdgeMs_ = millis();
  pressStartedMs_ = held_ ? lastEdgeMs_ : 0;
  lastHoldDurationMs_ = 0;
}

void ButtonHandler::update(uint32_t nowMs) {
  pressedEvent_ = false;
  releasedEvent_ = false;

  const bool currentHeld = !digitalRead(pin_);  // Board buttons are active-low.
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

bool ButtonHandler::isHeld() const { return held_; }

bool ButtonHandler::wasPressedEvent() const { return pressedEvent_; }

bool ButtonHandler::wasReleasedEvent() const { return releasedEvent_; }

uint32_t ButtonHandler::lastEdgeMs() const { return lastEdgeMs_; }

uint32_t ButtonHandler::heldDurationMs(uint32_t nowMs) const {
  return held_ ? nowMs - pressStartedMs_ : 0;
}

uint32_t ButtonHandler::lastHoldDurationMs() const { return lastHoldDurationMs_; }
