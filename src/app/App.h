#pragma once

#include <Arduino.h>

#include "app/AppState.h"
#include "display/DisplayManager.h"
#include "input/ButtonHandler.h"
#include "input/TouchHandler.h"
#include "storage/StorageManager.h"

class App {
 public:
  App();

  void begin();
  void update(uint32_t nowMs);

 private:
  void updateState();
  void handleTouch(uint32_t nowMs);

  AppState state_ = AppState::Booting;
  DisplayManager display_;
  ButtonHandler button_;
  TouchHandler touch_;
  StorageManager storage_;

  uint32_t lastStateLogMs_ = 0;
};
