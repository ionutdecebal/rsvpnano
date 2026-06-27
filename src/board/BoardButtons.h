#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Board::Buttons {

bool readVirtualBootHeld();
bool readVirtualPowerHeld();
bool consumeVirtualPowerShortPress();
bool consumeVirtualPowerLongPress();
bool usesPowerEvents();
uint32_t powerEventIgnoreMs();

}  // namespace Board::Buttons
