#pragma once

#include <cstdint>
#include <driver/gpio.h>

enum esp_sleep_wakeup_cause_t {
    ESP_SLEEP_WAKEUP_UNDEFINED,
    ESP_SLEEP_WAKEUP_GPIO,
    ESP_SLEEP_WAKEUP_TIMER
};
esp_err_t esp_sleep_enable_gpio_wakeup();
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t microseconds);
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t source);
esp_err_t esp_light_sleep_start();
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause();
