#pragma once

#include <Arduino.h>

namespace Board::Audio {

    bool begin();
    bool beep();
    bool available();

} // namespace Board::Audio
