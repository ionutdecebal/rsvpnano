#include "drivers/display/co5300/co5300.h"

#include <Arduino.h>
#include <SPI.h>
#include <driver/spi_master.h>
#include <esp_log.h>

namespace {

constexpr int kSpiFrequency = 20000000;
constexpr uint8_t kRamWriteCommand = 0x2C;
constexpr uint8_t kRamWriteContinueCommand = 0x3C;
static const char *kCo5300Tag = "co5300";

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[4];
  uint8_t len;
  uint16_t delayMs;
};

constexpr LcdCommand kQspiInit[] = {
    {0x11, {0x00}, 0, 120},
    {0xFE, {0x20}, 1, 0},
    {0x19, {0x10}, 1, 0},
    {0x1C, {0xA0}, 1, 0},
    {0xFE, {0x00}, 1, 0},
    {0xC4, {0x80}, 1, 0},
    {0x3A, {0x55}, 1, 0},
    {0x35, {0x00}, 1, 0},
    {0x53, {0x20}, 1, 0},
    {0x51, {0xFF}, 1, 0},
    {0x63, {0xFF}, 1, 0},
    {0x36, {0x00}, 1, 0},
};

uint8_t defaultMadctl(const Co5300::Context &context) {
  return context.config.panelMemoryRotated180 ? 0xC0 : 0x00;
}

size_t sendBufferPixels(const Co5300::Context &context) {
  constexpr size_t kFallbackPixels = 0x4000;
  const size_t rowBytes = static_cast<size_t>(context.config.panelWidth) * sizeof(uint16_t);
  if (rowBytes == 0 || context.config.txChunkBytes < rowBytes) {
    return context.config.panelWidth == 0 ? kFallbackPixels : context.config.panelWidth;
  }

  return context.config.panelWidth * (context.config.txChunkBytes / rowBytes);
}

void sendCommand(Co5300::Context &context, uint8_t command, const uint8_t *data,
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

void setColumnWindow(Co5300::Context &context, uint16_t x1, uint16_t x2) {
  x1 = static_cast<uint16_t>(x1 + context.config.columnOffset);
  x2 = static_cast<uint16_t>(x2 + context.config.columnOffset);
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(context, 0x2A, data, sizeof(data));
}

void setRowWindow(Co5300::Context &context, uint16_t y1, uint16_t y2) {
  y1 = static_cast<uint16_t>(y1 + context.config.rowOffset);
  y2 = static_cast<uint16_t>(y2 + context.config.rowOffset);
  const uint8_t data[] = {
      static_cast<uint8_t>(y1 >> 8),
      static_cast<uint8_t>(y1),
      static_cast<uint8_t>(y2 >> 8),
      static_cast<uint8_t>(y2),
  };
  sendCommand(context, 0x2B, data, sizeof(data));
}

void applyBrightness(Co5300::Context &context) {
  const uint8_t level =
      context.displayOn
          ? static_cast<uint8_t>((static_cast<uint16_t>(context.brightnessPercent) * 255U) / 100U)
          : 0;
  sendCommand(context, 0x51, &level, 1);
}

}  // namespace

namespace Co5300 {

void init(Context &context) {
  if (context.config.panelWidth == 0 || context.config.panelHeight == 0) {
    ESP_LOGE(kCo5300Tag, "Invalid CO5300 panel geometry");
    return;
  }

  if (context.config.resetPin >= 0) {
    pinMode(context.config.resetPin, OUTPUT);
    digitalWrite(context.config.resetPin, HIGH);
    delay(10);
    digitalWrite(context.config.resetPin, LOW);
    delay(200);
    digitalWrite(context.config.resetPin, HIGH);
    delay(200);
  }

  if (!context.busReady) {
    spi_bus_config_t busConfig = {};
    busConfig.data0_io_num = context.config.data0Pin;
    busConfig.data1_io_num = context.config.data1Pin;
    busConfig.sclk_io_num = context.config.sclkPin;
    busConfig.data2_io_num = context.config.data2Pin;
    busConfig.data3_io_num = context.config.data3Pin;
    busConfig.max_transfer_sz =
        static_cast<int>(sendBufferPixels(context) * sizeof(uint16_t)) + 8;
    busConfig.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = 8;
    deviceConfig.address_bits = 24;
    deviceConfig.mode = SPI_MODE0;
    deviceConfig.clock_speed_hz = kSpiFrequency;
    deviceConfig.spics_io_num = context.config.csPin;
    deviceConfig.flags = SPI_DEVICE_HALFDUPLEX;
    deviceConfig.queue_size = 10;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &context.spi));
    context.busReady = true;
  }

  for (const auto &command : kQspiInit) {
    uint8_t data[4] = {command.data[0], command.data[1], command.data[2], command.data[3]};
    if (command.cmd == 0x36 && command.len == 1) {
      data[0] = defaultMadctl(context);
    }
    sendCommand(context, command.cmd, data, command.len);
    if (command.delayMs != 0) {
      delay(command.delayMs);
    }
  }

  setColumnWindow(context, 0, static_cast<uint16_t>(context.config.panelWidth - 1));
  setRowWindow(context, 0, static_cast<uint16_t>(context.config.panelHeight - 1));

  context.displayOn = false;
  applyBrightness(context);
  ESP_LOGI(kCo5300Tag, "CO5300 QSPI init complete");
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
    const size_t maxChunkPixels = sendBufferPixels(context);
    if (chunkPixels > maxChunkPixels) {
      chunkPixels = maxChunkPixels;
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

}  // namespace Co5300
