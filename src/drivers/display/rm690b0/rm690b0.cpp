#include "drivers/display/rm690b0/rm690b0.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/spi_master.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {

constexpr int kSpiFrequency = 20000000;
constexpr int kSendBufferPixels = 0x4000;
constexpr int kColumnOffset = 16;
constexpr uint8_t kRamWriteCommand = 0x2C;
constexpr uint8_t kRamWriteContinueCommand = 0x3C;
constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
constexpr uint8_t kDisplayEnableExioPin = 1;
static const char *kRm690b0Tag = "rm690b0";

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[4];
  uint8_t len;
  uint16_t delayMs;
};

// Keep the panel in its native memory orientation and let the shared display
// mapping layer perform the quarter-turn into landscape. That avoids the edge
// wrap/clipping artifacts we saw when both layers rotated at once.
constexpr uint8_t kDefaultMadctl = Board::Config::UI_ROTATED_180 ? 0x10 : 0x00;
constexpr LcdCommand kQspiInit[] = {
    {0xFE, {0x20}, 1, 0},
    {0x24, {0x80}, 1, 0},
    {0x5B, {0x2E}, 1, 0},
    {0xFE, {0x00}, 1, 0},
    {0xC2, {0x00}, 1, 10},
    {0x35, {0x00}, 1, 0},
    {0x36, {kDefaultMadctl}, 1, 0},
    {0x3A, {0x55}, 1, 0},
    {0x80, {0x00}, 1, 0},
    {0x11, {0x00}, 0, 120},
    {0x29, {0x00}, 0, 10},
};

bool tca9554Read(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(Board::Config::TCA9554_ADDRESS);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }

  if (Wire1.requestFrom(static_cast<uint8_t>(Board::Config::TCA9554_ADDRESS),
                        static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire1.read();
  return true;
}

bool tca9554Write(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(Board::Config::TCA9554_ADDRESS);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission(true) == 0;
}

void enableDisplayRailIfAvailable() {
  if (Board::Config::TCA9554_ADDRESS < 0) {
    return;
  }

  uint8_t output = 0;
  uint8_t config = 0xFF;
  if (!tca9554Read(kTca9554OutputReg, output) || !tca9554Read(kTca9554ConfigReg, config)) {
    ESP_LOGW(kRm690b0Tag, "Display power expander not detected");
    return;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << kDisplayEnableExioPin);
  output |= mask;
  config &= static_cast<uint8_t>(~mask);
  if (!tca9554Write(kTca9554OutputReg, output) || !tca9554Write(kTca9554ConfigReg, config)) {
    ESP_LOGW(kRm690b0Tag, "Failed to enable display rail");
    return;
  }

  delay(25);
}

void sendCommand(Rm690b0::Context &context, uint8_t command, const uint8_t *data,
                 uint32_t length) {
  if (context.spi == nullptr) {
    return;
  }

  spi_transaction_t transaction = {};
  transaction.cmd = 0x02;
  transaction.addr = static_cast<uint32_t>(command) << 8;
  if (length != 0) {
    transaction.tx_buffer = data;
    transaction.length = length * 8;
  }

  ESP_ERROR_CHECK(spi_device_polling_transmit(context.spi, &transaction));
}

void setColumnWindow(Rm690b0::Context &context, uint16_t x1, uint16_t x2) {
  x1 = static_cast<uint16_t>(x1 + kColumnOffset);
  x2 = static_cast<uint16_t>(x2 + kColumnOffset);
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(context, 0x2A, data, sizeof(data));
}

void setRowWindow(Rm690b0::Context &context, uint16_t y1, uint16_t y2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(y1 >> 8),
      static_cast<uint8_t>(y1),
      static_cast<uint8_t>(y2 >> 8),
      static_cast<uint8_t>(y2),
  };
  sendCommand(context, 0x2B, data, sizeof(data));
}

void applyBrightness(Rm690b0::Context &context) {
  const uint8_t level =
      context.displayOn
          ? static_cast<uint8_t>((static_cast<uint16_t>(context.brightnessPercent) * 255U) / 100U)
          : 0;
  sendCommand(context, 0x51, &level, 1);
}

}  // namespace

namespace Rm690b0 {

void init(Context &context) {
  enableDisplayRailIfAvailable();

  pinMode(Board::Config::PIN_LCD_RST, OUTPUT);
  digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
  delay(30);
  digitalWrite(Board::Config::PIN_LCD_RST, LOW);
  delay(150);
  digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
  delay(150);

  if (!context.busReady) {
    spi_bus_config_t busConfig = {};
    busConfig.data0_io_num = Board::Config::PIN_LCD_DATA0;
    busConfig.data1_io_num = Board::Config::PIN_LCD_DATA1;
    busConfig.sclk_io_num = Board::Config::PIN_LCD_SCLK;
    busConfig.data2_io_num = Board::Config::PIN_LCD_DATA2;
    busConfig.data3_io_num = Board::Config::PIN_LCD_DATA3;
    busConfig.max_transfer_sz = (kSendBufferPixels * static_cast<int>(sizeof(uint16_t))) + 8;
    busConfig.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = 8;
    deviceConfig.address_bits = 24;
    deviceConfig.mode = SPI_MODE0;
    deviceConfig.clock_speed_hz = kSpiFrequency;
    deviceConfig.spics_io_num = Board::Config::PIN_LCD_CS;
    deviceConfig.flags = SPI_DEVICE_HALFDUPLEX;
    deviceConfig.queue_size = 10;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &context.spi));
    context.busReady = true;
  }

  for (const auto &command : kQspiInit) {
    sendCommand(context, command.cmd, command.data, command.len);
    if (command.delayMs != 0) {
      delay(command.delayMs);
    }
  }

  context.displayOn = true;
  applyBrightness(context);
  ESP_LOGI(kRm690b0Tag, "RM690B0 QSPI init complete");
}

void setDisplayOn(Context &context, bool on) {
  context.displayOn = on;
  sendCommand(context, on ? 0x29 : 0x28, nullptr, 0);
  applyBrightness(context);
}

void setBrightnessPercent(Context &context, uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  context.brightnessPercent = percent;
  applyBrightness(context);
}

void sleep(Context &context) {
  context.displayOn = false;
  sendCommand(context, 0x28, nullptr, 0);
  delay(10);
  sendCommand(context, 0x10, nullptr, 0);
  delay(5);
}

void wake(Context &context) {
  sendCommand(context, 0x11, nullptr, 0);
  delay(120);
  context.displayOn = true;
  sendCommand(context, 0x29, nullptr, 0);
  applyBrightness(context);
}

void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  if (context.spi == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  bool firstSend = true;
  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;

  setColumnWindow(context, x, static_cast<uint16_t>(x + width - 1));
  setRowWindow(context, y, static_cast<uint16_t>(y + height - 1));
  sendCommand(context, kRamWriteCommand, nullptr, 0);

  while (pixelsRemaining > 0) {
    size_t chunkPixels = pixelsRemaining;
    if (chunkPixels > static_cast<size_t>(kSendBufferPixels)) {
      chunkPixels = kSendBufferPixels;
    }

    spi_transaction_ext_t transaction = {};
    transaction.base.flags = SPI_TRANS_MODE_QIO;
    transaction.base.cmd = 0x32;
    transaction.base.addr = static_cast<uint32_t>(firstSend ? kRamWriteCommand
                                                            : kRamWriteContinueCommand)
                            << 8;

    transaction.base.tx_buffer = cursor;
    transaction.base.length = chunkPixels * 16;

    ESP_ERROR_CHECK(
        spi_device_polling_transmit(context.spi,
                                    reinterpret_cast<spi_transaction_t *>(&transaction)));

    firstSend = false;
    pixelsRemaining -= chunkPixels;
    cursor += chunkPixels;
  }
}

}  // namespace Rm690b0
