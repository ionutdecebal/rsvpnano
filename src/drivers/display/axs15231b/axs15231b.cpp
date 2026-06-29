#include "drivers/display/axs15231b/axs15231b.h"

#include <Arduino.h>
#include <SPI.h>
#include <driver/spi_master.h>
#include <esp_log.h>

namespace {

constexpr int kSpiFrequency = 40000000;
static const char *kAxs15231bTag = "axs15231b";

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[4];
  uint8_t len;
  uint16_t delayMs;
};

constexpr LcdCommand kQspiInit[] = {
    {0x11, {0x00}, 0, 100},
    {0x36, {0x00}, 1, 0},
    {0x3A, {0x55}, 1, 0},
    {0x11, {0x00}, 0, 100},
    {0x29, {0x00}, 0, 100},
};

void writeBacklightPwm(Axs15231b::Context &context) {
  if (context.config.backlightPin < 0) {
    return;
  }

  pinMode(context.config.backlightPin, OUTPUT);
  analogWriteResolution(8);
  // AP3032 CTRL PWM dimming:
  // Datasheet recommends high-frequency PWM to avoid audio noise,
  // and the figure labels the control signal around/up to 25 kHz.
  // Use 25 kHz instead of 50 kHz for better board compatibility.
  analogWriteFrequency(25000);

  if (!context.backlightOn) {
    analogWrite(context.config.backlightPin, 255);
    return;
  }

  // Waveshare drives the LCD backlight as active-low PWM; lower duty is brighter.
  const uint8_t brightness = context.brightnessPercent == 0 ? 1 : context.brightnessPercent;
  const uint8_t activeDuty =
      static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 255U) / 100U);
  analogWrite(context.config.backlightPin, 255 - activeDuty);
}

size_t sendBufferPixels(const Axs15231b::Context &context) {
  constexpr size_t kFallbackPixels = 0x4000;
  return context.config.txChunkBytes == 0
             ? kFallbackPixels
             : context.config.txChunkBytes / sizeof(uint16_t);
}

void setBacklight(Axs15231b::Context &context, bool on) {
  context.backlightOn = on;
  writeBacklightPwm(context);
}

void sendCommand(Axs15231b::Context &context, uint8_t command, const uint8_t *data,
                 uint32_t length) {
  if (context.spi == nullptr) {
    return;
  }

  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  transaction.cmd = 0x02;
  transaction.addr = static_cast<uint32_t>(command) << 8;
  if (length != 0) {
    transaction.tx_buffer = data;
    transaction.length = length * 8;
  }

  ESP_ERROR_CHECK(spi_device_polling_transmit(context.spi, &transaction));
}

void setColumnWindow(Axs15231b::Context &context, uint16_t x1, uint16_t x2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(context, 0x2A, data, sizeof(data));
}

}  // namespace

namespace Axs15231b {

void init(Context &context) {
  setBacklight(context, false);

  if (context.config.resetPin >= 0) {
    pinMode(context.config.resetPin, OUTPUT);
    digitalWrite(context.config.resetPin, HIGH);
    delay(30);
    digitalWrite(context.config.resetPin, LOW);
    delay(250);
    digitalWrite(context.config.resetPin, HIGH);
    delay(30);
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
    deviceConfig.mode = SPI_MODE3;
    deviceConfig.clock_speed_hz = kSpiFrequency;
    deviceConfig.spics_io_num = context.config.csPin;
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

  ESP_LOGI(kAxs15231bTag, "AXS15231B QSPI init complete");
}

void setBacklight(Context &context, bool on) { ::setBacklight(context, on); }

void setBrightnessPercent(Context &context, uint8_t percent) {
  if (percent == 0) {
    percent = 1;
  } else if (percent > 100) {
    percent = 100;
  }

  context.brightnessPercent = percent;
  writeBacklightPwm(context);
}

void sleep(Context &context) {
  // The panel can wake to a lit-but-blank state after AXS15231B SLPIN on this board.
  // For light sleep, blank the frame before this call and only switch off the backlight.
  setBacklight(context, false);
}

void wake(Context &context) {
  sendCommand(context, 0x11, nullptr, 0);
  delay(100);
  sendCommand(context, 0x29, nullptr, 0);
  setBacklight(context, true);
}

void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  if (context.spi == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  bool firstSend = true;
  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;

  setColumnWindow(context, x, x + width - 1);

  while (pixelsRemaining > 0) {
    size_t chunkPixels = pixelsRemaining;
    const size_t maxChunkPixels = sendBufferPixels(context);
    if (chunkPixels > maxChunkPixels) {
      chunkPixels = maxChunkPixels;
    }

    spi_transaction_ext_t transaction = {};
    if (firstSend) {
      transaction.base.flags = SPI_TRANS_MODE_QIO;
      transaction.base.cmd = 0x32;
      transaction.base.addr = y == 0 ? 0x002C00 : 0x003C00;
      firstSend = false;
    } else {
      transaction.base.flags =
          SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
          SPI_TRANS_VARIABLE_DUMMY;
      transaction.command_bits = 0;
      transaction.address_bits = 0;
      transaction.dummy_bits = 0;
    }

    transaction.base.tx_buffer = cursor;
    transaction.base.length = chunkPixels * 16;

    ESP_ERROR_CHECK(
        spi_device_polling_transmit(context.spi,
                                    reinterpret_cast<spi_transaction_t *>(&transaction)));

    pixelsRemaining -= chunkPixels;
    cursor += chunkPixels;
  }
}

}  // namespace Axs15231b
