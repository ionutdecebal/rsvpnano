#include "drivers/audio/es8311/Es8311.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace BoardDrivers::Es8311 {
namespace {

constexpr char kTag[] = "es8311";

constexpr uint8_t kResetReg = 0x00;
constexpr uint8_t kClkManagerReg01 = 0x01;
constexpr uint8_t kClkManagerReg02 = 0x02;
constexpr uint8_t kClkManagerReg03 = 0x03;
constexpr uint8_t kClkManagerReg04 = 0x04;
constexpr uint8_t kClkManagerReg05 = 0x05;
constexpr uint8_t kClkManagerReg06 = 0x06;
constexpr uint8_t kClkManagerReg07 = 0x07;
constexpr uint8_t kClkManagerReg08 = 0x08;
constexpr uint8_t kSdPinReg09 = 0x09;
constexpr uint8_t kSdPoutReg0A = 0x0A;
constexpr uint8_t kSystemReg0B = 0x0B;
constexpr uint8_t kSystemReg0C = 0x0C;
constexpr uint8_t kSystemReg0D = 0x0D;
constexpr uint8_t kSystemReg0E = 0x0E;
constexpr uint8_t kSystemReg10 = 0x10;
constexpr uint8_t kSystemReg11 = 0x11;
constexpr uint8_t kSystemReg12 = 0x12;
constexpr uint8_t kSystemReg13 = 0x13;
constexpr uint8_t kSystemReg14 = 0x14;
constexpr uint8_t kAdcReg15 = 0x15;
constexpr uint8_t kAdcReg16 = 0x16;
constexpr uint8_t kAdcReg17 = 0x17;
constexpr uint8_t kAdcReg1B = 0x1B;
constexpr uint8_t kAdcReg1C = 0x1C;
constexpr uint8_t kDacReg31 = 0x31;
constexpr uint8_t kDacReg32 = 0x32;
constexpr uint8_t kDacReg37 = 0x37;
constexpr uint8_t kGpioReg44 = 0x44;
constexpr uint8_t kGpReg45 = 0x45;
constexpr uint8_t kChipId1RegFD = 0xFD;
constexpr uint8_t kChipId2RegFE = 0xFE;
constexpr uint8_t kDacVolumeMax = 0xFF;

bool readRegister(Context &context, uint8_t reg, uint8_t &value) {
  if (context.wire == nullptr) {
    return false;
  }

  context.wire->beginTransmission(context.address);
  context.wire->write(reg);
  if (context.wire->endTransmission(false) != 0) {
    return false;
  }
  if (context.wire->requestFrom(static_cast<int>(context.address), 1, 1) != 1) {
    return false;
  }

  value = context.wire->read();
  return true;
}

bool writeRegister(Context &context, uint8_t reg, uint8_t value) {
  if (context.wire == nullptr) {
    return false;
  }

  context.wire->beginTransmission(context.address);
  context.wire->write(reg);
  context.wire->write(value);
  return context.wire->endTransmission(true) == 0;
}

bool initI2s(Context &context) {
  if (context.i2sInitialized) {
    return true;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = context.sampleRateHz;
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

  esp_err_t result = i2s_driver_install(context.i2sPort, &config, 0, nullptr);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to install I2S driver: %s", esp_err_to_name(result));
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.mck_io_num = context.mclkPin;
  pinConfig.bck_io_num = context.bclkPin;
  pinConfig.ws_io_num = context.wsPin;
  pinConfig.data_out_num = context.dataOutPin;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(context.i2sPort, &pinConfig);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S pins: %s", esp_err_to_name(result));
    return false;
  }

  result =
      i2s_set_clk(context.i2sPort, context.sampleRateHz, I2S_BITS_PER_SAMPLE_16BIT,
                  I2S_CHANNEL_STEREO);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S clock: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(context.i2sPort);
  context.i2sInitialized = true;
  return true;
}

bool detectCodec(Context &context) {
  uint8_t chipId1 = 0;
  uint8_t chipId2 = 0;
  if (readRegister(context, kChipId1RegFD, chipId1) &&
      readRegister(context, kChipId2RegFE, chipId2)) {
    ESP_LOGI(kTag, "ES8311 detected: id=%02X %02X", chipId1, chipId2);
  }
  return true;
}

bool configureSampleFormat(Context &context) {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t reg = 0;

  if (!readRegister(context, kSdPinReg09, dacIface) ||
      !readRegister(context, kSdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface = static_cast<uint8_t>((dacIface & 0xE0U) | 0x0CU);
  adcIface = static_cast<uint8_t>((adcIface & 0xE0U) | 0x0CU);

  return writeRegister(context, kSdPinReg09, dacIface) &&
         writeRegister(context, kSdPoutReg0A, adcIface) &&
         readRegister(context, kClkManagerReg02, reg) &&
         writeRegister(context, kClkManagerReg02, static_cast<uint8_t>(reg & 0x07U)) &&
         writeRegister(context, kClkManagerReg05, 0x00) &&
         readRegister(context, kClkManagerReg03, reg) &&
         writeRegister(context, kClkManagerReg03, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readRegister(context, kClkManagerReg04, reg) &&
         writeRegister(context, kClkManagerReg04, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readRegister(context, kClkManagerReg07, reg) &&
         writeRegister(context, kClkManagerReg07, static_cast<uint8_t>(reg & 0xC0U)) &&
         writeRegister(context, kClkManagerReg08, 0xFF) &&
         readRegister(context, kClkManagerReg06, reg) &&
         writeRegister(context, kClkManagerReg06, static_cast<uint8_t>((reg & 0xE0U) | 0x03U));
}

bool startCodec(Context &context) {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t dacMute = 0;

  if (!writeRegister(context, kResetReg, 0x80) ||
      !writeRegister(context, kClkManagerReg01, 0x3F) ||
      !readRegister(context, kSdPinReg09, dacIface) ||
      !readRegister(context, kSdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface &= static_cast<uint8_t>(~(1U << 6));
  adcIface |= static_cast<uint8_t>(1U << 6);

  const bool started =
      writeRegister(context, kSdPinReg09, dacIface) &&
      writeRegister(context, kSdPoutReg0A, adcIface) &&
      writeRegister(context, kAdcReg17, 0xBF) && writeRegister(context, kSystemReg0E, 0x02) &&
      writeRegister(context, kSystemReg12, 0x00) && writeRegister(context, kSystemReg14, 0x1A) &&
      writeRegister(context, kSystemReg0D, 0x01) && writeRegister(context, kAdcReg15, 0x40) &&
      writeRegister(context, kDacReg37, 0x08) && writeRegister(context, kGpReg45, 0x00);

  if (!started || !readRegister(context, kDacReg31, dacMute)) {
    return false;
  }

  dacMute &= 0x9F;
  return writeRegister(context, kDacReg31, dacMute) &&
         writeRegister(context, kDacReg32, kDacVolumeMax);
}

bool configureCodec(Context &context) {
  uint8_t reg = 0;

  const bool opened =
      writeRegister(context, kGpioReg44, 0x08) && writeRegister(context, kGpioReg44, 0x08) &&
      writeRegister(context, kClkManagerReg01, 0x30) &&
      writeRegister(context, kClkManagerReg02, 0x00) &&
      writeRegister(context, kClkManagerReg03, 0x10) &&
      writeRegister(context, kAdcReg16, 0x24) &&
      writeRegister(context, kClkManagerReg04, 0x10) &&
      writeRegister(context, kClkManagerReg05, 0x00) &&
      writeRegister(context, kSystemReg0B, 0x00) && writeRegister(context, kSystemReg0C, 0x00) &&
      writeRegister(context, kSystemReg10, 0x1F) && writeRegister(context, kSystemReg11, 0x7F) &&
      writeRegister(context, kResetReg, 0x80) && readRegister(context, kResetReg, reg) &&
      writeRegister(context, kResetReg, static_cast<uint8_t>(reg & 0xBF)) &&
      writeRegister(context, kClkManagerReg01, 0x3F) &&
      readRegister(context, kClkManagerReg06, reg) &&
      writeRegister(context, kClkManagerReg06, static_cast<uint8_t>(reg & ~0x20U)) &&
      writeRegister(context, kSystemReg13, 0x10) && writeRegister(context, kAdcReg1B, 0x0A) &&
      writeRegister(context, kAdcReg1C, 0x6A) && writeRegister(context, kGpioReg44, 0x58);

  return opened && configureSampleFormat(context) && startCodec(context);
}

}  // namespace

bool begin(Context &context) {
  if (context.available) {
    return true;
  }

  if (!initI2s(context) || !detectCodec(context) || !configureCodec(context)) {
    ESP_LOGW(kTag, "Audio codec setup failed");
    return false;
  }

  context.available = true;
  ESP_LOGI(kTag, "Speaker path ready");
  return true;
}

bool prepareOutput(Context &context) {
  if (!context.available || !context.i2sInitialized) {
    return false;
  }

  uint8_t dacMute = 0;
  if (readRegister(context, kDacReg31, dacMute)) {
    dacMute &= 0x9F;
    writeRegister(context, kDacReg31, dacMute);
  }
  writeRegister(context, kDacReg32, kDacVolumeMax);
  return true;
}

bool recoverOutputPath(Context &context) {
  if (!startCodec(context) || !context.i2sInitialized) {
    return false;
  }

  i2s_stop(context.i2sPort);
  const esp_err_t result = i2s_start(context.i2sPort);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to restart I2S TX: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(context.i2sPort);
  return true;
}

bool writeSamples(Context &context, const int16_t *samples, size_t sampleCount,
                  uint32_t timeoutMs) {
  if (samples == nullptr || sampleCount == 0) {
    return false;
  }

  const uint8_t *data = reinterpret_cast<const uint8_t *>(samples);
  const size_t totalSize = sampleCount * sizeof(int16_t);
  size_t totalWritten = 0;

  while (totalWritten < totalSize) {
    size_t bytesWritten = 0;
    const esp_err_t result =
        i2s_write(context.i2sPort, data + totalWritten, totalSize - totalWritten, &bytesWritten,
                  pdMS_TO_TICKS(timeoutMs));
    if (result != ESP_OK || bytesWritten == 0) {
      ESP_LOGW(kTag, "Sample write failed: %s", esp_err_to_name(result));
      return false;
    }
    totalWritten += bytesWritten;
  }

  return true;
}

bool available(const Context &context) { return context.available; }

}  // namespace BoardDrivers::Es8311
