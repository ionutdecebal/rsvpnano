#pragma once

#include "../../../support/driver/gpio.h"

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;

enum gpio_int_type_t {
    GPIO_INTR_DISABLE,
    GPIO_INTR_NEGEDGE,
    GPIO_INTR_LOW_LEVEL
};
esp_err_t gpio_wakeup_enable(gpio_num_t pin, gpio_int_type_t type);
esp_err_t gpio_wakeup_disable(gpio_num_t pin);
esp_err_t gpio_intr_disable(gpio_num_t pin);
esp_err_t gpio_set_intr_type(gpio_num_t pin, gpio_int_type_t type);
