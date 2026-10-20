#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace ui {

    enum class Orientation : uint8_t {
        Portrait = 0,
        LandscapeFlipped = 1,
        PortraitFlipped = 2,
        Landscape = 3,
    };

    constexpr Orientation opposite(Orientation orientation) {
        return static_cast<Orientation>((static_cast<uint8_t>(orientation) + 2U) & 3U);
    }

    enum TouchAction : uint8_t {
        TouchNone = 0,
        TouchStart = 1U << 0,
        TouchMove = 1U << 1,
        TouchRelease = 1U << 2,
        TouchTap = 1U << 3,
        TouchHold = 1U << 4,
        TouchCancel = 1U << 5,
    };

    struct Touch {
        uint8_t actions = TouchNone;
        uint16_t x = 0;
        uint16_t y = 0;
    };

    constexpr bool hasTouch(const Touch& touch, TouchAction action) {
        return (touch.actions & action) != 0;
    }

    constexpr int32_t centeredDragRate(int16_t position, int16_t origin, int16_t length, int16_t deadzone,
                                       int32_t maximum) {
        const int32_t half = std::max<int16_t>(1, length / 2);
        const int32_t distance = std::clamp<int32_t>(position - (origin + half), -half, half);
        const int32_t inactive = std::min<int32_t>(deadzone, half - 1);
        if (std::abs(distance) <= inactive)
            return 0;
        const int32_t active = std::abs(distance) - inactive;
        const int32_t range = half - inactive;
        const int32_t rate = static_cast<int32_t>(static_cast<int64_t>(maximum) * active * active / (range * range));
        return distance < 0 ? -rate : rate;
    }

    struct TouchContact {
        bool touched = false;
        uint16_t x = 0;
        uint16_t y = 0;
        uint32_t sampledAtMs = 0;
    };

    struct TouchSurface {
        uint16_t width = 0;
        uint16_t height = 0;
    };

    struct TouchTiming {
        uint16_t tapMoveTolerancePx = 20;
        uint16_t tapMaxDurationMs = 600;
        uint16_t holdMs = 600;
    };

    enum class TouchSampleResult : uint8_t {
        None,
        Contact,
        Reset,
    };

    struct TouchSource {
        TouchSurface surface;
        TouchTiming timing;
        TouchSampleResult (*poll)(TouchContact&) = nullptr;
    };

} // namespace ui
