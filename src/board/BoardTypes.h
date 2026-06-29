#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Board {

    enum class UiOrientation : uint8_t {
        Landscape = 0,
        LandscapeFlipped,
        Portrait,
        PortraitFlipped,
    };

    constexpr UiOrientation oppositeUiOrientation(UiOrientation orientation) {
        switch (orientation) {
        case UiOrientation::Landscape:
            return UiOrientation::LandscapeFlipped;
        case UiOrientation::LandscapeFlipped:
            return UiOrientation::Landscape;
        case UiOrientation::Portrait:
            return UiOrientation::PortraitFlipped;
        case UiOrientation::PortraitFlipped:
        default:
            return UiOrientation::Portrait;
        }
    }

    struct BatteryStatus {
        bool present = false;
        float voltage = 0.0f;
        uint8_t percent = 0;
    };

    struct PowerDiagnosticSnapshot {
        bool available = false;
        bool externalPowerPresent = false;
        uint8_t status1 = 0;
        uint8_t status2 = 0;
        uint8_t powerKeyIrqStatus = 0;
    };

} // namespace Board
