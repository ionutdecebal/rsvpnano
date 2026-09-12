#include "sleep/EspLightSleep.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include <algorithm>
#include <esp_log.h>

namespace {

    constexpr uint32_t kReleasePollMs = 10;

} // namespace

namespace EspLightSleep {

    WakeReason wait(std::span<const gpio_num_t> wakePins, uint32_t timeoutMs) {
        {
            for (const gpio_num_t pin: wakePins)
                pinMode(static_cast<int>(pin), INPUT_PULLUP);

            while (true) {
                const bool allReleased = std::ranges::all_of(wakePins, [](gpio_num_t pin) {
                    return digitalRead(static_cast<int>(pin));
                });
                if (allReleased)
                    break;
                delay(kReleasePollMs);
            }
        }

        const auto disableWakeSources = [&] {
            for (const gpio_num_t pin: wakePins) {
                gpio_wakeup_disable(pin);
                // Wake setup changes the normal GPIO trigger too. Leave it disabled until
                // input resumes/rearms; otherwise another pinMode can enable a level ISR.
                gpio_intr_disable(pin);
                gpio_set_intr_type(pin, GPIO_INTR_DISABLE);
            }
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
            if (timeoutMs > 0)
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        };

        {
            for (const gpio_num_t pin: wakePins) {
                if (const esp_err_t error = gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL); error != ESP_OK) {
                    ESP_LOGE("sleep", "failed to arm GPIO %d for light sleep: %d", static_cast<int>(pin),
                             static_cast<int>(error));
                    disableWakeSources();
                    return WakeReason::error;
                }
            }

            const esp_err_t gpioError = esp_sleep_enable_gpio_wakeup();
            const esp_err_t timerError =
                timeoutMs == 0 ? ESP_OK : esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(timeoutMs) * 1000ULL);
            if (gpioError != ESP_OK || timerError != ESP_OK) {
                ESP_LOGE("sleep", "failed to arm light sleep: gpio=%d timer=%d", static_cast<int>(gpioError),
                         static_cast<int>(timerError));
                disableWakeSources();
                return WakeReason::error;
            }
        }

        const esp_err_t sleepError = esp_light_sleep_start();
        const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        disableWakeSources();

        if (sleepError != ESP_OK) {
            ESP_LOGE("sleep", "light sleep failed: %d", static_cast<int>(sleepError));
            return WakeReason::error;
        }

        switch (cause) {
        case ESP_SLEEP_WAKEUP_GPIO:
            return WakeReason::input;
        case ESP_SLEEP_WAKEUP_TIMER:
            return WakeReason::timer;
        default:
            ESP_LOGE("sleep", "unexpected wakeup cause: %d", static_cast<int>(cause));
            return WakeReason::error;
        }
    }

} // namespace EspLightSleep
