#include "display/axs15231b.h"

#include <driver/spi_master.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {

constexpr int kSpiFrequency = 40000000;
constexpr int kSendBufferPixels = 0x4000;
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

spi_device_handle_t gSpi = nullptr;
bool gBusReady = false;
bool gBacklightOn = false;
uint8_t gBrightnessPercent = 100;

void writeBacklightPwm() {
  pinMode(BoardConfig::PIN_LCD_BACKLIGHT, OUTPUT);
  analogWriteResolution(8);
  analogWriteFrequency(50000);

  if (!gBacklightOn) {
    analogWrite(BoardConfig::PIN_LCD_BACKLIGHT, 255);
    return;
  }

  // Waveshare drives the LCD backlight as active-low PWM; lower duty is brighter.
  const uint8_t brightness = gBrightnessPercent == 0 ? 1 : gBrightnessPercent;
  const uint8_t activeDuty =
      static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 255U) / 100U);
  analogWrite(BoardConfig::PIN_LCD_BACKLIGHT, 255 - activeDuty);
}

void setBacklight(bool on) {
  gBacklightOn = on;
  writeBacklightPwm();
}

void sendCommand(uint8_t command, const uint8_t *data, uint32_t length) {
  if (gSpi == nullptr) {
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

  ESP_ERROR_CHECK(spi_device_polling_transmit(gSpi, &transaction));
}

void setColumnWindow(uint16_t x1, uint16_t x2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(0x2A, data, sizeof(data));
}

}  // namespace

void axs15231bInit() {
  setBacklight(false);

  pinMode(BoardConfig::PIN_LCD_RST, OUTPUT);
  digitalWrite(BoardConfig::PIN_LCD_RST, HIGH);
  delay(30);
  digitalWrite(BoardConfig::PIN_LCD_RST, LOW);
  delay(250);
  digitalWrite(BoardConfig::PIN_LCD_RST, HIGH);
  delay(30);

  if (!gBusReady) {
    spi_bus_config_t busConfig = {};
    busConfig.data0_io_num = BoardConfig::PIN_LCD_DATA0;
    busConfig.data1_io_num = BoardConfig::PIN_LCD_DATA1;
    busConfig.sclk_io_num = BoardConfig::PIN_LCD_SCLK;
    busConfig.data2_io_num = BoardConfig::PIN_LCD_DATA2;
    busConfig.data3_io_num = BoardConfig::PIN_LCD_DATA3;
    busConfig.max_transfer_sz = (kSendBufferPixels * static_cast<int>(sizeof(uint16_t))) + 8;
    busConfig.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = 8;
    deviceConfig.address_bits = 24;
    deviceConfig.mode = SPI_MODE3;
    deviceConfig.clock_speed_hz = kSpiFrequency;
    deviceConfig.spics_io_num = BoardConfig::PIN_LCD_CS;
    deviceConfig.flags = SPI_DEVICE_HALFDUPLEX;
    deviceConfig.queue_size = 10;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &gSpi));
    gBusReady = true;
  }

  for (const auto &command : kQspiInit) {
    sendCommand(command.cmd, command.data, command.len);
    if (command.delayMs != 0) {
      delay(command.delayMs);
    }
  }

  ESP_LOGI(kAxs15231bTag, "AXS15231B QSPI init complete");
}

void axs15231bSetBacklight(bool on) { setBacklight(on); }

void axs15231bSetBrightnessPercent(uint8_t percent) {
  if (percent == 0) {
    percent = 1;
  } else if (percent > 100) {
    percent = 100;
  }

  gBrightnessPercent = percent;
  writeBacklightPwm();
}

void axs15231bSleep() {
  // The panel can wake to a lit-but-blank state after AXS15231B SLPIN on this board.
  // For light sleep, blank the frame before this call and only switch off the backlight.
  setBacklight(false);
}

void axs15231bWake() {
  sendCommand(0x11, nullptr, 0);
  delay(100);
  sendCommand(0x29, nullptr, 0);
  setBacklight(true);
}

void axs15231bPushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                         const uint16_t *data) {
  if (gSpi == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  bool firstSend = true;
  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;

  setColumnWindow(x, x + width - 1);

  while (pixelsRemaining > 0) {
    size_t chunkPixels = pixelsRemaining;
    if (chunkPixels > static_cast<size_t>(kSendBufferPixels)) {
      chunkPixels = kSendBufferPixels;
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
        spi_device_polling_transmit(gSpi, reinterpret_cast<spi_transaction_t *>(&transaction)));

    pixelsRemaining -= chunkPixels;
    cursor += chunkPixels;
  }
}
