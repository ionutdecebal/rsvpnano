#include "display/jd9853.h"

#include <sdkconfig.h>

#if CONFIG_IDF_TARGET_ESP32C6

#include <Arduino.h>
#include <SPI.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {

constexpr uint32_t kSpiFrequency = 40000000;
constexpr int kSendBufferPixels = 0x2000;
// Panel is a 172×320 visible window inside the 240×320 JD9853 controller; the
// 172-wide region is centered, so the column axis carries a +34 offset.
constexpr int kColumnOffset = 34;
constexpr int kRowOffset = 0;
// Drive the panel in its native portrait orientation. DisplayManager::flushScaledFrame
// already rotates the logical landscape bitmap into a 172-wide column stream before
// pushing it, so a hardware rotation here would double-rotate the image.
// MX=1 flips the column scan to undo the horizontal mirror that arises from how the
// 1.47" panel is mounted on the Waveshare board relative to the controller's default
// scan direction. If the image still looks mirrored, try 0x80 (MY) or 0xC0 (MX+MY)
// instead.
constexpr uint8_t kMadctlPortrait = 0x40;

static const char *kJd9853Tag = "jd9853";

const SPISettings kLcdSpiSettings(kSpiFrequency, MSBFIRST, SPI_MODE0);

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[16];
  uint8_t len;
  uint16_t delayMs;
};

// Init sequence taken from the Waveshare JD9853 reference (172x320 panel).
constexpr LcdCommand kJd9853Init[] = {
    {0x11, {0}, 0, 120},                                                          // Sleep out
    {0x36, {kMadctlPortrait}, 1, 0},                                              // MADCTL
    {0x3A, {0x05}, 1, 0},                                                         // COLMOD = 16-bit/pixel
    {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5, 0},                                 // Porch control
    {0xB7, {0x35}, 1, 0},                                                         // Gate control
    {0xBB, {0x35}, 1, 0},                                                         // VCOM
    {0xC0, {0x2C}, 1, 0},                                                         // LCMCTRL
    {0xC2, {0x01}, 1, 0},                                                         // VDV/VRH enable
    {0xC3, {0x13}, 1, 0},                                                         // VRH set
    {0xC4, {0x20}, 1, 0},                                                         // VDV set
    {0xC6, {0x0F}, 1, 0},                                                         // Frame rate
    {0xD0, {0xA4, 0xA1}, 2, 0},                                                   // Power control
    {0xD6, {0xA1}, 1, 0},
    {0xE0,
     {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30},
     14, 0},                                                                      // Gamma + curves
    {0xE1,
     {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32},
     14, 0},
    {0x21, {0}, 0, 0},                                                            // Inversion ON
    {0x29, {0}, 0, 20},                                                           // Display ON
};

bool gBusReady = false;
bool gBacklightOn = false;
uint8_t gBrightnessPercent = 100;

void writeBacklightPwm() {
  pinMode(BoardConfig::PIN_LCD_BACKLIGHT, OUTPUT);
  analogWriteResolution(BoardConfig::PIN_LCD_BACKLIGHT, 8);
  analogWriteFrequency(BoardConfig::PIN_LCD_BACKLIGHT, 50000);

  if (!gBacklightOn) {
    analogWrite(BoardConfig::PIN_LCD_BACKLIGHT,
                BoardConfig::LCD_BACKLIGHT_ACTIVE_LOW ? 255 : 0);
    return;
  }

  const uint8_t brightness = gBrightnessPercent == 0 ? 1 : gBrightnessPercent;
  const uint8_t activeDuty =
      static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 255U) / 100U);
  const uint8_t duty = BoardConfig::LCD_BACKLIGHT_ACTIVE_LOW ? (255 - activeDuty) : activeDuty;
  analogWrite(BoardConfig::PIN_LCD_BACKLIGHT, duty);
}

void setBacklight(bool on) {
  gBacklightOn = on;
  writeBacklightPwm();
}

inline void dcCommand() { digitalWrite(BoardConfig::PIN_LCD_DC, LOW); }
inline void dcData() { digitalWrite(BoardConfig::PIN_LCD_DC, HIGH); }
inline void csLow() { digitalWrite(BoardConfig::PIN_LCD_CS, LOW); }
inline void csHigh() { digitalWrite(BoardConfig::PIN_LCD_CS, HIGH); }

void sendCommand(uint8_t command, const uint8_t *data, uint32_t length) {
  if (!gBusReady) {
    return;
  }

  SPI.beginTransaction(kLcdSpiSettings);
  csLow();
  dcCommand();
  SPI.transfer(command);
  if (length != 0 && data != nullptr) {
    dcData();
    SPI.writeBytes(data, length);
  }
  csHigh();
  SPI.endTransaction();
}

void setAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  const uint16_t x1 = x + kColumnOffset;
  const uint16_t x2 = x + w - 1 + kColumnOffset;
  const uint16_t y1 = y + kRowOffset;
  const uint16_t y2 = y + h - 1 + kRowOffset;
  const uint8_t casetData[] = {
      static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0xFF),
      static_cast<uint8_t>(x2 >> 8), static_cast<uint8_t>(x2 & 0xFF),
  };
  const uint8_t rasetData[] = {
      static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0xFF),
      static_cast<uint8_t>(y2 >> 8), static_cast<uint8_t>(y2 & 0xFF),
  };
  sendCommand(0x2A, casetData, sizeof(casetData));
  sendCommand(0x2B, rasetData, sizeof(rasetData));
  sendCommand(0x2C, nullptr, 0);  // RAMWR — keeps DC=command for the marker, then we switch to data.
}

}  // namespace

void jd9853Init() {
  setBacklight(false);

  pinMode(BoardConfig::PIN_LCD_CS, OUTPUT);
  digitalWrite(BoardConfig::PIN_LCD_CS, HIGH);
  pinMode(BoardConfig::PIN_LCD_DC, OUTPUT);
  digitalWrite(BoardConfig::PIN_LCD_DC, HIGH);
  pinMode(BoardConfig::PIN_LCD_RST, OUTPUT);
  digitalWrite(BoardConfig::PIN_LCD_RST, HIGH);
  delay(20);
  digitalWrite(BoardConfig::PIN_LCD_RST, LOW);
  delay(150);
  digitalWrite(BoardConfig::PIN_LCD_RST, HIGH);
  delay(150);

  if (!gBusReady) {
    // The TF (SD) slot shares this SPI bus on the Waveshare 1.47 board
    // (LCD_SCLK=IO1, LCD_MOSI=IO2 are also SD CLK/MOSI). Initialise Arduino's
    // SPI here with MISO wired to the SD card's data-out so the SD library
    // can later use the same bus. Each peripheral manages its own CS pin and
    // arbitrates through Arduino SPI's transaction mutex.
    SPI.begin(BoardConfig::PIN_LCD_SCLK, BoardConfig::PIN_SD_D0,
              BoardConfig::PIN_LCD_MOSI, -1);
    gBusReady = true;
  }

  for (const auto &command : kJd9853Init) {
    sendCommand(command.cmd, command.data, command.len);
    if (command.delayMs != 0) {
      delay(command.delayMs);
    }
  }

  ESP_LOGI(kJd9853Tag, "JD9853 init complete");
}

void jd9853SetBacklight(bool on) { setBacklight(on); }

void jd9853SetBrightnessPercent(uint8_t percent) {
  if (percent == 0) {
    percent = 1;
  } else if (percent > 100) {
    percent = 100;
  }
  gBrightnessPercent = percent;
  writeBacklightPwm();
}

void jd9853Sleep() {
  setBacklight(false);
  sendCommand(0x28, nullptr, 0);  // Display off
  sendCommand(0x10, nullptr, 0);  // Sleep in
  delay(120);
}

void jd9853Wake() {
  sendCommand(0x11, nullptr, 0);
  delay(120);
  sendCommand(0x29, nullptr, 0);
  setBacklight(true);
}

void jd9853PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                      const uint16_t *data) {
  if (!gBusReady || data == nullptr || width == 0 || height == 0) {
    return;
  }

  setAddressWindow(x, y, width, height);

  SPI.beginTransaction(kLcdSpiSettings);
  csLow();
  dcData();

  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;
  while (pixelsRemaining > 0) {
    size_t chunkPixels = pixelsRemaining;
    if (chunkPixels > static_cast<size_t>(kSendBufferPixels)) {
      chunkPixels = kSendBufferPixels;
    }

    SPI.writeBytes(reinterpret_cast<const uint8_t *>(cursor), chunkPixels * 2);

    pixelsRemaining -= chunkPixels;
    cursor += chunkPixels;
  }

  csHigh();
  SPI.endTransaction();
}

#endif  // CONFIG_IDF_TARGET_ESP32C6
