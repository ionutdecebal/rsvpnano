#include "app/App.h"

#include "board/BoardConfig.h"

App::App() : button_(BoardConfig::PIN_BOOT_BUTTON) {}

void App::begin() {
  BoardConfig::begin();
  button_.begin();
  display_.begin();
  touch_.begin();
  storage_.begin();

  display_.renderCenteredWord("READY");
  storage_.listBooks();

  state_ = button_.isHeld() ? AppState::Playing : AppState::Paused;
}

void App::update(uint32_t nowMs) {
  button_.update(nowMs);
  updateState();
  handleTouch(nowMs);

  // TODO: App heartbeat/log throttling can be expanded if needed for diagnostics.
  if (nowMs - lastStateLogMs_ > 1500) {
    lastStateLogMs_ = nowMs;
    Serial.printf("[app] state=%d\n", static_cast<int>(state_));
  }
}

void App::updateState() {
  const AppState nextState = button_.isHeld() ? AppState::Playing : AppState::Paused;
  if (nextState == state_) {
    return;
  }

  state_ = nextState;
  if (state_ == AppState::Playing) {
    display_.renderCenteredWord("PLAY");
  } else if (state_ == AppState::Paused) {
    display_.renderCenteredWord("PAUSE");
  }

  // TODO: Add transitions for Menu and Sleeping states when those features are implemented.
}

void App::handleTouch(uint32_t nowMs) {
  (void)nowMs;
  TouchEvent ev;
  if (!touch_.poll(ev)) {
    return;
  }

  Serial.printf("[touch] gesture=%u x=%u y=%u state=%d\n", ev.gesture, ev.x, ev.y,
                static_cast<int>(state_));

  if (state_ == AppState::Playing) {
    // Interaction model requirement: ignore touch while playing.
    return;
  }

  // TODO: While paused, map raw touch events to swipe/long-press actions.
}
