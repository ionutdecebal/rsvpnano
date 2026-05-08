#include "audio/AudioManager.h"

#include <Wire.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board/BoardConfig.h"

namespace {

constexpr char kTag[] = "audio";

constexpr uint8_t kIoConfigRegister = 0x03;
constexpr uint8_t kIoOutputRegister = 0x01;

constexpr uint8_t kEs8311ResetReg = 0x00;
constexpr uint8_t kEs8311ClkManagerReg01 = 0x01;
constexpr uint8_t kEs8311ClkManagerReg02 = 0x02;
constexpr uint8_t kEs8311ClkManagerReg03 = 0x03;
constexpr uint8_t kEs8311ClkManagerReg04 = 0x04;
constexpr uint8_t kEs8311ClkManagerReg05 = 0x05;
constexpr uint8_t kEs8311ClkManagerReg06 = 0x06;
constexpr uint8_t kEs8311ClkManagerReg07 = 0x07;
constexpr uint8_t kEs8311ClkManagerReg08 = 0x08;
constexpr uint8_t kEs8311SdPinReg09 = 0x09;
constexpr uint8_t kEs8311SdPoutReg0A = 0x0A;
constexpr uint8_t kEs8311SystemReg0B = 0x0B;
constexpr uint8_t kEs8311SystemReg0C = 0x0C;
constexpr uint8_t kEs8311SystemReg0D = 0x0D;
constexpr uint8_t kEs8311SystemReg0E = 0x0E;
constexpr uint8_t kEs8311SystemReg10 = 0x10;
constexpr uint8_t kEs8311SystemReg11 = 0x11;
constexpr uint8_t kEs8311SystemReg12 = 0x12;
constexpr uint8_t kEs8311SystemReg13 = 0x13;
constexpr uint8_t kEs8311SystemReg14 = 0x14;
constexpr uint8_t kEs8311AdcReg15 = 0x15;
constexpr uint8_t kEs8311AdcReg16 = 0x16;
constexpr uint8_t kEs8311AdcReg17 = 0x17;
constexpr uint8_t kEs8311AdcReg1B = 0x1B;
constexpr uint8_t kEs8311AdcReg1C = 0x1C;
constexpr uint8_t kEs8311DacReg31 = 0x31;
constexpr uint8_t kEs8311DacReg32 = 0x32;
constexpr uint8_t kEs8311DacReg37 = 0x37;
constexpr uint8_t kEs8311GpioReg44 = 0x44;
constexpr uint8_t kEs8311GpReg45 = 0x45;
constexpr uint8_t kEs8311ChipId1RegFD = 0xFD;
constexpr uint8_t kEs8311ChipId2RegFE = 0xFE;

constexpr uint8_t kEs8311DacVolumeMax = 0xFF;
constexpr uint32_t kAudioStartupDelayMs = 15;
constexpr uint32_t kBeepFrequencyHz = 1320;
constexpr int16_t kBeepAmplitude = 12000;
constexpr uint32_t kEnvelopeAttackMs = 6;
constexpr uint32_t kEnvelopeReleaseMs = 12;

}  // namespace

bool AudioManager::begin() {
  if (available_) {
    return true;
  }

  fillBeepBuffer();

  if (!enableAudioRail()) {
    ESP_LOGW(kTag, "Audio rail unavailable");
    return false;
  }

  delay(kAudioStartupDelayMs);

  if (!initI2s() || !initCodec() || !configureCodec()) {
    ESP_LOGW(kTag, "Audio codec setup failed");
    return false;
  }

  available_ = true;
  ESP_LOGI(kTag, "Speaker path ready");
  return true;
}

bool AudioManager::beep() {
  if (!prepareForBeep()) {
    return false;
  }

  if (writeBeepBuffer()) {
    return true;
  }

  ESP_LOGW(kTag, "Retrying speaker beep after recovering output path");
  if (!recoverOutputPath()) {
    return false;
  }

  return writeBeepBuffer();
}

bool AudioManager::available() const { return available_; }

bool AudioManager::enableAudioRail() {
  uint8_t direction = 0xFF;
  uint8_t output = 0xFF;
  if (!readIoRegister(kIoConfigRegister, direction) || !readIoRegister(kIoOutputRegister, output)) {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << BoardConfig::TCA9554_PIN_AUDIO_ENABLE);
  output |= mask;
  direction &= static_cast<uint8_t>(~mask);

  return writeIoRegister(kIoOutputRegister, output) &&
         writeIoRegister(kIoConfigRegister, direction);
}

bool AudioManager::initI2s() {
  if (i2sInitialized_) {
    return true;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = kSampleRateHz;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 4;
  config.dma_buf_len = 240;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  esp_err_t result = i2s_driver_install(kI2sPort, &config, 0, nullptr);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to install I2S driver: %s", esp_err_to_name(result));
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.mck_io_num = BoardConfig::PIN_AUDIO_MCLK;
  pinConfig.bck_io_num = BoardConfig::PIN_AUDIO_BCLK;
  pinConfig.ws_io_num = BoardConfig::PIN_AUDIO_WS;
  pinConfig.data_out_num = BoardConfig::PIN_AUDIO_DOUT;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(kI2sPort, &pinConfig);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S pins: %s", esp_err_to_name(result));
    return false;
  }

  result = i2s_set_clk(kI2sPort, kSampleRateHz, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S clock: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(kI2sPort);
  i2sInitialized_ = true;
  return true;
}

bool AudioManager::initCodec() {
  uint8_t chipId1 = 0;
  uint8_t chipId2 = 0;
  if (readCodecRegister(kEs8311ChipId1RegFD, chipId1) &&
      readCodecRegister(kEs8311ChipId2RegFE, chipId2)) {
    ESP_LOGI(kTag, "ES8311 detected: id=%02X %02X", chipId1, chipId2);
  }
  return true;
}

bool AudioManager::configureCodec() {
  uint8_t reg = 0;

  const bool opened =
      writeCodecRegister(kEs8311GpioReg44, 0x08) &&
      writeCodecRegister(kEs8311GpioReg44, 0x08) &&
      writeCodecRegister(kEs8311ClkManagerReg01, 0x30) &&
      writeCodecRegister(kEs8311ClkManagerReg02, 0x00) &&
      writeCodecRegister(kEs8311ClkManagerReg03, 0x10) &&
      writeCodecRegister(kEs8311AdcReg16, 0x24) &&
      writeCodecRegister(kEs8311ClkManagerReg04, 0x10) &&
      writeCodecRegister(kEs8311ClkManagerReg05, 0x00) &&
      writeCodecRegister(kEs8311SystemReg0B, 0x00) &&
      writeCodecRegister(kEs8311SystemReg0C, 0x00) &&
      writeCodecRegister(kEs8311SystemReg10, 0x1F) &&
      writeCodecRegister(kEs8311SystemReg11, 0x7F) &&
      writeCodecRegister(kEs8311ResetReg, 0x80) &&
      readCodecRegister(kEs8311ResetReg, reg) &&
      writeCodecRegister(kEs8311ResetReg, static_cast<uint8_t>(reg & 0xBF)) &&
      writeCodecRegister(kEs8311ClkManagerReg01, 0x3F) &&
      readCodecRegister(kEs8311ClkManagerReg06, reg) &&
      writeCodecRegister(kEs8311ClkManagerReg06, static_cast<uint8_t>(reg & ~0x20U)) &&
      writeCodecRegister(kEs8311SystemReg13, 0x10) &&
      writeCodecRegister(kEs8311AdcReg1B, 0x0A) &&
      writeCodecRegister(kEs8311AdcReg1C, 0x6A) &&
      writeCodecRegister(kEs8311GpioReg44, 0x58);

  if (!opened) {
    return false;
  }

  return configureCodecSampleFormat() && startCodec();
}

bool AudioManager::configureCodecSampleFormat() {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t reg = 0;

  if (!readCodecRegister(kEs8311SdPinReg09, dacIface) ||
      !readCodecRegister(kEs8311SdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface = static_cast<uint8_t>((dacIface & 0xE0U) | 0x0CU);
  adcIface = static_cast<uint8_t>((adcIface & 0xE0U) | 0x0CU);

  return writeCodecRegister(kEs8311SdPinReg09, dacIface) &&
         writeCodecRegister(kEs8311SdPoutReg0A, adcIface) &&
         readCodecRegister(kEs8311ClkManagerReg02, reg) &&
         writeCodecRegister(kEs8311ClkManagerReg02, static_cast<uint8_t>(reg & 0x07U)) &&
         writeCodecRegister(kEs8311ClkManagerReg05, 0x00) &&
         readCodecRegister(kEs8311ClkManagerReg03, reg) &&
         writeCodecRegister(kEs8311ClkManagerReg03, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readCodecRegister(kEs8311ClkManagerReg04, reg) &&
         writeCodecRegister(kEs8311ClkManagerReg04, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readCodecRegister(kEs8311ClkManagerReg07, reg) &&
         writeCodecRegister(kEs8311ClkManagerReg07, static_cast<uint8_t>(reg & 0xC0U)) &&
         writeCodecRegister(kEs8311ClkManagerReg08, 0xFF) &&
         readCodecRegister(kEs8311ClkManagerReg06, reg) &&
         writeCodecRegister(kEs8311ClkManagerReg06, static_cast<uint8_t>((reg & 0xE0U) | 0x03U));
}

bool AudioManager::startCodec() {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t dacMute = 0;

  if (!writeCodecRegister(kEs8311ResetReg, 0x80) ||
      !writeCodecRegister(kEs8311ClkManagerReg01, 0x3F) ||
      !readCodecRegister(kEs8311SdPinReg09, dacIface) ||
      !readCodecRegister(kEs8311SdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface &= static_cast<uint8_t>(~(1U << 6));
  adcIface |= static_cast<uint8_t>(1U << 6);

  const bool started =
      writeCodecRegister(kEs8311SdPinReg09, dacIface) &&
      writeCodecRegister(kEs8311SdPoutReg0A, adcIface) &&
      writeCodecRegister(kEs8311AdcReg17, 0xBF) &&
      writeCodecRegister(kEs8311SystemReg0E, 0x02) &&
      writeCodecRegister(kEs8311SystemReg12, 0x00) &&
      writeCodecRegister(kEs8311SystemReg14, 0x1A) &&
      writeCodecRegister(kEs8311SystemReg0D, 0x01) &&
      writeCodecRegister(kEs8311AdcReg15, 0x40) &&
      writeCodecRegister(kEs8311DacReg37, 0x08) &&
      writeCodecRegister(kEs8311GpReg45, 0x00);

  if (!started || !readCodecRegister(kEs8311DacReg31, dacMute)) {
    return false;
  }

  dacMute &= 0x9F;
  return writeCodecRegister(kEs8311DacReg31, dacMute) &&
         writeCodecRegister(kEs8311DacReg32, kEs8311DacVolumeMax);
}

bool AudioManager::prepareForBeep() {
  if (!available_ || !i2sInitialized_) {
    return false;
  }

  if (!enableAudioRail()) {
    return false;
  }

  uint8_t dacMute = 0;
  if (readCodecRegister(kEs8311DacReg31, dacMute)) {
    dacMute &= 0x9F;
    writeCodecRegister(kEs8311DacReg31, dacMute);
  }
  writeCodecRegister(kEs8311DacReg32, kEs8311DacVolumeMax);
  return true;
}

bool AudioManager::recoverOutputPath() {
  if (!enableAudioRail() || !startCodec() || !i2sInitialized_) {
    return false;
  }

  i2s_stop(kI2sPort);
  const esp_err_t result = i2s_start(kI2sPort);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to restart I2S TX: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(kI2sPort);
  return true;
}

bool AudioManager::writeBeepBuffer() {
  const uint8_t *data = reinterpret_cast<const uint8_t *>(beepBuffer_);
  size_t totalWritten = 0;
  const size_t totalSize = sizeof(beepBuffer_);

  while (totalWritten < totalSize) {
    size_t bytesWritten = 0;
    const esp_err_t result = i2s_write(kI2sPort, data + totalWritten, totalSize - totalWritten,
                                       &bytesWritten, pdMS_TO_TICKS(250));
    if (result != ESP_OK || bytesWritten == 0) {
      ESP_LOGW(kTag, "Beep write failed: %s", esp_err_to_name(result));
      return false;
    }
    totalWritten += bytesWritten;
  }

  return true;
}

bool AudioManager::readIoRegister(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(BoardConfig::TCA9554_ADDRESS);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }
  if (Wire1.requestFrom(static_cast<int>(BoardConfig::TCA9554_ADDRESS), 1, 1) != 1) {
    return false;
  }

  value = Wire1.read();
  return true;
}

bool AudioManager::writeIoRegister(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(BoardConfig::TCA9554_ADDRESS);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission(true) == 0;
}

bool AudioManager::readCodecRegister(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(BoardConfig::ES8311_ADDRESS);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }
  if (Wire1.requestFrom(static_cast<int>(BoardConfig::ES8311_ADDRESS), 1, 1) != 1) {
    return false;
  }

  value = Wire1.read();
  return true;
}

bool AudioManager::writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(BoardConfig::ES8311_ADDRESS);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission(true) == 0;
}

void AudioManager::fillBeepBuffer() {
  const size_t attackFrames = (static_cast<size_t>(kSampleRateHz) * kEnvelopeAttackMs) / 1000U;
  const size_t releaseFrames = (static_cast<size_t>(kSampleRateHz) * kEnvelopeReleaseMs) / 1000U;
  const uint32_t halfPeriodSamples = kSampleRateHz / (kBeepFrequencyHz * 2U);

  for (size_t frame = 0; frame < kBeepFrames; ++frame) {
    int32_t sample =
        ((frame / halfPeriodSamples) % 2U == 0U) ? kBeepAmplitude : -kBeepAmplitude;

    if (attackFrames > 0 && frame < attackFrames) {
      sample = (sample * static_cast<int32_t>(frame)) / static_cast<int32_t>(attackFrames);
    } else if (releaseFrames > 0 && frame >= (kBeepFrames - releaseFrames)) {
      const size_t remaining = kBeepFrames - frame;
      sample = (sample * static_cast<int32_t>(remaining)) / static_cast<int32_t>(releaseFrames);
    }

    const size_t index = frame * 2U;
    beepBuffer_[index] = static_cast<int16_t>(sample);
    beepBuffer_[index + 1U] = static_cast<int16_t>(sample);
  }
}
