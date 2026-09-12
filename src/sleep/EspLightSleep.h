#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <driver/gpio.h>

namespace EspLightSleep {

    enum class WakeReason : uint8_t {
        input,
        timer,
        error,
    };

    // Call before input starts or while its interrupts are detached. Resume input after waking.
    WakeReason wait(std::span<const gpio_num_t> wakePins, uint32_t timeoutMs);

    template<gpio_num_t InputPin, int TouchPin>
    WakeReason wait(uint32_t timeoutMs) {
        if constexpr (TouchPin >= 0) {
            constexpr std::array wakePins = {InputPin, static_cast<gpio_num_t>(TouchPin)};
            return wait(wakePins, timeoutMs);
        } else {
            constexpr std::array wakePins = {InputPin};
            return wait(wakePins, timeoutMs);
        }
    }

} // namespace EspLightSleep
