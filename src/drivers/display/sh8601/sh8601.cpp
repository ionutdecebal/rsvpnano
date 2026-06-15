#include "drivers/display/sh8601/sh8601.h"

#include <Arduino.h>
#include <SPI.h>
#include <driver/spi_master.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {

constexpr int kSpiFrequency = 20000000;
constexpr int kSendBufferRows =
    Board::Config::DISPLAY_TX_CHUNK_BYTES /
    (Board::Config::PANEL_NATIVE_WIDTH * static_cast<int>(sizeof(uint16_t)));
static_assert(kSendBufferRows > 0, "SH8601 transfer buffer must hold at least one full row");
constexpr int kSendBufferPixels = Board::Config::PANEL_NATIVE_WIDTH * kSendBufferRows;
constexpr uint8_t kRamWriteCommand = 0x2C;
constexpr uint8_t kRamWriteContinueCommand = 0x3C;
static const char *kSh8601Tag = "sh8601";

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[4];
  uint8_t len;
  uint16_t delayMs;
};

// Keep the SH8601 memory in its unmirrored native orientation and let the shared display mapping
// handle the quarter-turn into landscape. The borrowed X-mirror bit from Espressif's sample left
// the whole UI horizontally mirrored on real hardware.
constexpr uint8_t kDefaultMadctl = 0x00;
constexpr LcdCommand kQspiInit[] = {
    {0x36, {kDefaultMadctl}, 1, 0},
    {0x3A, {0x55}, 1, 0},
    {0x11, {0x00}, 0, 120},
    {0x44, {0x01, 0xD1}, 2, 0},
    {0x35, {0x00}, 1, 0},
    {0x53, {0x20}, 1, 10},
    {0x2A, {0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, {0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x29, {0x00}, 0, 10},
};

void sendCommand(Sh8601::Context &context, uint8_t command, const uint8_t *data,
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

void setColumnWindow(Sh8601::Context &context, uint16_t x1, uint16_t x2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(context, 0x2A, data, sizeof(data));
}

void setRowWindow(Sh8601::Context &context, uint16_t y1, uint16_t y2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(y1 >> 8),
      static_cast<uint8_t>(y1),
      static_cast<uint8_t>(y2 >> 8),
      static_cast<uint8_t>(y2),
  };
  sendCommand(context, 0x2B, data, sizeof(data));
}

void applyBrightness(Sh8601::Context &context) {
  const uint8_t level =
      context.displayOn
          ? static_cast<uint8_t>((static_cast<uint16_t>(context.brightnessPercent) * 255U) / 100U)
          : 0;
  sendCommand(context, 0x51, &level, 1);
}

}  // namespace

namespace Sh8601 {

void init(Context &context) {
  if (Board::Config::PIN_LCD_RST >= 0) {
    pinMode(Board::Config::PIN_LCD_RST, OUTPUT);
    digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
    delay(10);
    digitalWrite(Board::Config::PIN_LCD_RST, LOW);
    delay(150);
    digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
    delay(150);
  }

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
  ESP_LOGI(kSh8601Tag, "SH8601 QSPI init complete");
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
  delay(120);
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

}  // namespace Sh8601
