#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

namespace BoardDrivers::Es8311 {

struct Context {
  Context() = default;

  Context(TwoWire *wire, uint8_t address, i2s_port_t i2sPort, int mclkPin, int bclkPin, int wsPin,
          int dataOutPin, uint32_t sampleRateHz = 16000)
      : wire(wire),
        address(address),
        i2sPort(i2sPort),
        mclkPin(mclkPin),
        bclkPin(bclkPin),
        wsPin(wsPin),
        dataOutPin(dataOutPin),
        sampleRateHz(sampleRateHz) {}

  TwoWire *wire = nullptr;
  uint8_t address = 0;
  i2s_port_t i2sPort = I2S_NUM_0;
  int mclkPin = I2S_PIN_NO_CHANGE;
  int bclkPin = I2S_PIN_NO_CHANGE;
  int wsPin = I2S_PIN_NO_CHANGE;
  int dataOutPin = I2S_PIN_NO_CHANGE;
  uint32_t sampleRateHz = 16000;
  bool available = false;
  bool i2sInitialized = false;
};

bool begin(Context &context);
bool prepareOutput(Context &context);
bool recoverOutputPath(Context &context);
bool writeSamples(Context &context, const int16_t *samples, size_t sampleCount, uint32_t timeoutMs);
bool available(const Context &context);

}  // namespace BoardDrivers::Es8311
