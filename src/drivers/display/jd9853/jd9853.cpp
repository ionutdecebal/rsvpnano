#include "drivers/display/jd9853/jd9853.h"

#include <algorithm>

#include <esp_log.h>

namespace {

constexpr uint32_t kSpiFrequency = 40000000;
constexpr uint8_t kRamWriteCommand = 0x2C;
static const char *kJd9853Tag = "jd9853";

struct LcdCommand {
  uint8_t cmd;
  const uint8_t *data;
  uint8_t len;
  uint16_t delayMs;
};

constexpr uint8_t kCmdDf[] = {0x98, 0x53};
constexpr uint8_t kCmdB2[] = {0x23};
constexpr uint8_t kCmdB7A[] = {0x00, 0x47, 0x00, 0x6F};
constexpr uint8_t kCmdBb[] = {0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0};
constexpr uint8_t kCmdC0[] = {0x44, 0xA4};
constexpr uint8_t kCmdC1A[] = {0x16};
constexpr uint8_t kCmdC3[] = {0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77};
constexpr uint8_t kCmdC4A[] = {0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A,
                               0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82};
constexpr uint8_t kCmdC8[] = {0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                              0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                              0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                              0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00};
constexpr uint8_t kCmdD0[] = {0x04, 0x06, 0x6B, 0x0F, 0x00};
constexpr uint8_t kCmdD7[] = {0x00, 0x30};
constexpr uint8_t kCmdE6[] = {0x14};
constexpr uint8_t kCmdDe1[] = {0x01};
constexpr uint8_t kCmdB7B[] = {0x03, 0x13, 0xEF, 0x35, 0x35};
constexpr uint8_t kCmdC1B[] = {0x14, 0x15, 0xC0};
constexpr uint8_t kCmdC2[] = {0x06, 0x3A};
constexpr uint8_t kCmdC4B[] = {0x72, 0x12};
constexpr uint8_t kCmdBe[] = {0x00};
constexpr uint8_t kCmdDe2[] = {0x02};
constexpr uint8_t kCmdE5A[] = {0x00, 0x02, 0x00};
constexpr uint8_t kCmdE5B[] = {0x01, 0x02, 0x00};
constexpr uint8_t kCmdDe0[] = {0x00};
constexpr uint8_t kCmd35[] = {0x00};
constexpr uint8_t kCmd3A[] = {0x05};
constexpr uint8_t kCmd2A[] = {0x00, 0x22, 0x00, 0xCD};
constexpr uint8_t kCmd2B[] = {0x00, 0x00, 0x01, 0x3F};

constexpr LcdCommand kInitCommands[] = {
    {0x11, nullptr, 0, 120}, {0xDF, kCmdDf, sizeof(kCmdDf), 0},
    {0xDF, kCmdDf, sizeof(kCmdDf), 0}, {0xB2, kCmdB2, sizeof(kCmdB2), 0},
    {0xB7, kCmdB7A, sizeof(kCmdB7A), 0}, {0xBB, kCmdBb, sizeof(kCmdBb), 0},
    {0xC0, kCmdC0, sizeof(kCmdC0), 0}, {0xC1, kCmdC1A, sizeof(kCmdC1A), 0},
    {0xC3, kCmdC3, sizeof(kCmdC3), 0}, {0xC4, kCmdC4A, sizeof(kCmdC4A), 0},
    {0xC8, kCmdC8, sizeof(kCmdC8), 0}, {0xD0, kCmdD0, sizeof(kCmdD0), 0},
    {0xD7, kCmdD7, sizeof(kCmdD7), 0}, {0xE6, kCmdE6, sizeof(kCmdE6), 0},
    {0xDE, kCmdDe1, sizeof(kCmdDe1), 0}, {0xB7, kCmdB7B, sizeof(kCmdB7B), 0},
    {0xC1, kCmdC1B, sizeof(kCmdC1B), 0}, {0xC2, kCmdC2, sizeof(kCmdC2), 0},
    {0xC4, kCmdC4B, sizeof(kCmdC4B), 0}, {0xBE, kCmdBe, sizeof(kCmdBe), 0},
    {0xDE, kCmdDe2, sizeof(kCmdDe2), 0}, {0xE5, kCmdE5A, sizeof(kCmdE5A), 0},
    {0xE5, kCmdE5B, sizeof(kCmdE5B), 0}, {0xDE, kCmdDe0, sizeof(kCmdDe0), 0},
    {0x35, kCmd35, sizeof(kCmd35), 0}, {0x3A, kCmd3A, sizeof(kCmd3A), 0},
    {0x2A, kCmd2A, sizeof(kCmd2A), 0}, {0x2B, kCmd2B, sizeof(kCmd2B), 0},
    {0xDE, kCmdDe2, sizeof(kCmdDe2), 0}, {0xE5, kCmdE5A, sizeof(kCmdE5A), 0},
    {0xDE, kCmdDe0, sizeof(kCmdDe0), 0}, {0x29, nullptr, 0, 0},
};

size_t chunkPixels(const Jd9853::Context &context) {
  constexpr size_t kFallbackPixels = 4096;
  const size_t chunkBytes = context.config.txChunkBytes;
  if (chunkBytes < sizeof(uint16_t)) {
    return kFallbackPixels;
  }
  return std::max<size_t>(1, chunkBytes / sizeof(uint16_t));
}

void select(const Jd9853::Context &context) { digitalWrite(context.config.csPin, LOW); }

void deselect(const Jd9853::Context &context) { digitalWrite(context.config.csPin, HIGH); }

void writeBytes(Jd9853::Context &context, const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  context.spi->writeBytes(data, len);
}

void sendCommand(Jd9853::Context &context, uint8_t command, const uint8_t *data, size_t len) {
  context.spi->beginTransaction(SPISettings(kSpiFrequency, MSBFIRST, SPI_MODE0));
  select(context);

  digitalWrite(context.config.dcPin, LOW);
  context.spi->write(command);
  if (len != 0) {
    digitalWrite(context.config.dcPin, HIGH);
    writeBytes(context, data, len);
  }

  deselect(context);
  context.spi->endTransaction();
}

void setWindow(Jd9853::Context &context, uint16_t x, uint16_t y, uint16_t width,
               uint16_t height) {
  const uint16_t x1 = static_cast<uint16_t>(x + context.config.columnOffset);
  const uint16_t x2 = static_cast<uint16_t>(x1 + width - 1);
  const uint16_t y1 = static_cast<uint16_t>(y + context.config.rowOffset);
  const uint16_t y2 = static_cast<uint16_t>(y1 + height - 1);

  const uint8_t column[] = {static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1),
                            static_cast<uint8_t>(x2 >> 8), static_cast<uint8_t>(x2)};
  const uint8_t row[] = {static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1),
                         static_cast<uint8_t>(y2 >> 8), static_cast<uint8_t>(y2)};
  sendCommand(context, 0x2A, column, sizeof(column));
  sendCommand(context, 0x2B, row, sizeof(row));
}

void applyBacklight(Jd9853::Context &context) {
  if (context.config.backlightPin < 0) {
    return;
  }

  const uint8_t duty =
      context.displayOn
          ? static_cast<uint8_t>((static_cast<uint16_t>(context.brightnessPercent) * 255U) / 100U)
          : 0;
  analogWrite(context.config.backlightPin, duty);
}

}  // namespace

namespace Jd9853 {

void init(Context &context) {
  pinMode(context.config.csPin, OUTPUT);
  pinMode(context.config.dcPin, OUTPUT);
  deselect(context);

  if (context.config.backlightPin >= 0) {
    pinMode(context.config.backlightPin, OUTPUT);
    analogWriteResolution(8);
    analogWriteFrequency(20000);
    analogWrite(context.config.backlightPin, 0);
  }

  if (context.config.resetPin >= 0) {
    pinMode(context.config.resetPin, OUTPUT);
    digitalWrite(context.config.resetPin, HIGH);
    delay(10);
    digitalWrite(context.config.resetPin, LOW);
    delay(10);
    digitalWrite(context.config.resetPin, HIGH);
    delay(120);
  }

  if (!context.busReady) {
    context.spi->begin(context.config.sclkPin, context.config.misoPin, context.config.mosiPin,
                       context.config.csPin);
    context.busReady = true;
  }

  for (const auto &command : kInitCommands) {
    sendCommand(context, command.cmd, command.data, command.len);
    if (command.delayMs != 0) {
      delay(command.delayMs);
    }
  }

  context.displayOn = true;
  applyBacklight(context);
  ESP_LOGI(kJd9853Tag, "JD9853 SPI init complete");
}

void setDisplayOn(Context &context, bool on) {
  context.displayOn = on;
  sendCommand(context, on ? 0x29 : 0x28, nullptr, 0);
  applyBacklight(context);
}

void setBrightnessPercent(Context &context, uint8_t percent) {
  context.brightnessPercent = std::min<uint8_t>(percent, 100);
  applyBacklight(context);
}

void sleep(Context &context) {
  context.displayOn = false;
  sendCommand(context, 0x28, nullptr, 0);
  delay(10);
  sendCommand(context, 0x10, nullptr, 0);
  delay(120);
  applyBacklight(context);
}

void wake(Context &context) {
  sendCommand(context, 0x11, nullptr, 0);
  delay(120);
  context.displayOn = true;
  sendCommand(context, 0x29, nullptr, 0);
  applyBacklight(context);
}

void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  if (data == nullptr || width == 0 || height == 0) {
    return;
  }

  setWindow(context, x, y, width, height);

  context.spi->beginTransaction(SPISettings(kSpiFrequency, MSBFIRST, SPI_MODE0));
  select(context);
  digitalWrite(context.config.dcPin, LOW);
  context.spi->write(kRamWriteCommand);
  digitalWrite(context.config.dcPin, HIGH);

  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;
  while (pixelsRemaining > 0) {
    const size_t pixels = std::min(pixelsRemaining, chunkPixels(context));
    writeBytes(context, reinterpret_cast<const uint8_t *>(cursor), pixels * sizeof(uint16_t));
    cursor += pixels;
    pixelsRemaining -= pixels;
  }

  deselect(context);
  context.spi->endTransaction();
}

}  // namespace Jd9853
