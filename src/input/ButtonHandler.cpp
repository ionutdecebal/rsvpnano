#include "input/ButtonHandler.h"

ButtonHandler::ButtonHandler(int pin) : pin_(pin) {}

void ButtonHandler::begin() {
  pinMode(pin_, INPUT_PULLUP);
  held_ = !digitalRead(pin_);
  lastEdgeMs_ = millis();
}

void ButtonHandler::update(uint32_t nowMs) {
  pressedEvent_ = false;

  const bool currentHeld = !digitalRead(pin_);  // Active-low BOOT button.
  if (currentHeld != held_) {
    held_ = currentHeld;
    lastEdgeMs_ = nowMs;
    if (held_) {
      pressedEvent_ = true;
    }
  }

  // TODO: Add multi-press detection here for triple-press sleep while paused.
}

bool ButtonHandler::isHeld() const { return held_; }

bool ButtonHandler::wasPressedEvent() const { return pressedEvent_; }

uint32_t ButtonHandler::lastEdgeMs() const { return lastEdgeMs_; }
