#pragma once

#include <Arduino.h>
#include <SPI.h>

namespace Jd9853 {

struct Config {
  int csPin = -1;
  int dcPin = -1;
  int sclkPin = -1;
  int mosiPin = -1;
  int misoPin = -1;
  int resetPin = -1;
  int backlightPin = -1;
  uint16_t panelWidth = 0;
  uint16_t panelHeight = 0;
  uint16_t columnOffset = 0;
  uint16_t rowOffset = 0;
  size_t txChunkBytes = 0;
};

struct Context {
  Config config;
  SPIClass *spi = &SPI;
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

}  // namespace Jd9853
