#pragma once

#include <Arduino.h>

#include "input/Input.h"

namespace Board::Input {

    bool begin();
    void end();
    void cancel();
    ::Input::ControlTiming controlTiming();
    ::Input::ControlMask currentControls();
    ::Input::TouchSurface touchSurface();
    ::Input::TouchTiming touchTiming();
    bool beginTouch();
    bool touchReady();
    bool readTouch(::Input::TouchContact& contact);

} // namespace Board::Input
