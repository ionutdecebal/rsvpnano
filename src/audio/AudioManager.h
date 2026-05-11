#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class AudioManager {
 public:
  bool begin();
  bool beep();
  bool available() const;
  void releaseI2s();
  bool prepareCodecForStreaming();

 private:
  static constexpr uint32_t kSampleRateHz = 16000;
  static constexpr uint32_t kBeepDurationMs = 120;
  static constexpr size_t kBeepFrames =
      (static_cast<size_t>(kSampleRateHz) * kBeepDurationMs) / 1000U;
  static constexpr size_t kBeepSamples = kBeepFrames * 2U;
  static constexpr i2s_port_t kI2sPort = I2S_NUM_0;

  bool enableAudioRail();
  bool initI2s();
  bool initCodec();
  bool configureCodec();
  bool configureCodecSampleFormat();
  bool startCodec();
  bool prepareForBeep();
  bool recoverOutputPath();
  bool writeBeepBuffer();
  bool readIoRegister(uint8_t reg, uint8_t &value);
  bool writeIoRegister(uint8_t reg, uint8_t value);
  bool readCodecRegister(uint8_t reg, uint8_t &value);
  bool writeCodecRegister(uint8_t reg, uint8_t value);
  void fillBeepBuffer();

  bool available_ = false;
  bool i2sInitialized_ = false;
  int16_t beepBuffer_[kBeepSamples] = {};
};
