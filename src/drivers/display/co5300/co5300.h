#pragma once

#include <stdint.h>
#include <driver/spi_master.h>

namespace Co5300 {

struct Context {
  spi_device_handle_t spi = nullptr;
  bool busReady = false;
  bool displayOn = true;
  uint8_t brightnessPercent = 100;
};

void init(Context &context);
void setDisplayOn(Context &context, bool on);
void setBrightnessPercent(Context &context, uint8_t percent);
void sleep(Context &context);
void wake(Context &context);
void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data);

}  // namespace Co5300
