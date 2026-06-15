#pragma once

#include <Arduino.h>
#include <driver/spi_master.h>

namespace Axs15231b {

struct Context {
  spi_device_handle_t spi = nullptr;
  bool busReady = false;
  bool backlightOn = false;
  uint8_t brightnessPercent = 100;
};

void init(Context &context);
void setBacklight(Context &context, bool on);
void setBrightnessPercent(Context &context, uint8_t percent);
void sleep(Context &context);
void wake(Context &context);
void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data);

}  // namespace Axs15231b
