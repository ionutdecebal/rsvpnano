#pragma once

#include <Arduino.h>
#include <sdkconfig.h>

namespace BoardConfig {

// Optional NTC thermistor for battery-pack temperature. Disabled by default on
// every target; opt in by adding `-DRSVP_THERMISTOR_PIN=<gpio>` (and optionally
// `-DRSVP_THERMISTOR_BETA=...`, `-DRSVP_THERMISTOR_R0_OHMS=...`,
// `-DRSVP_THERMISTOR_SERIES_OHMS=...`, `-DRSVP_THERMISTOR_NTC_TO_GND=0|1`) to
// the env's build_flags. The pin must be a free ADC1 channel.
#ifdef RSVP_THERMISTOR_PIN
constexpr int PIN_THERMISTOR_ADC = RSVP_THERMISTOR_PIN;
#else
constexpr int PIN_THERMISTOR_ADC = -1;
#endif
#ifndef RSVP_THERMISTOR_SERIES_OHMS
#define RSVP_THERMISTOR_SERIES_OHMS 10000.0f
#endif
#ifndef RSVP_THERMISTOR_R0_OHMS
#define RSVP_THERMISTOR_R0_OHMS 10000.0f
#endif
#ifndef RSVP_THERMISTOR_BETA
#define RSVP_THERMISTOR_BETA 3950.0f
#endif
#ifndef RSVP_THERMISTOR_NTC_TO_GND
#define RSVP_THERMISTOR_NTC_TO_GND 1
#endif
constexpr float THERMISTOR_SERIES_OHMS = RSVP_THERMISTOR_SERIES_OHMS;
constexpr float THERMISTOR_NOMINAL_OHMS = RSVP_THERMISTOR_R0_OHMS;
constexpr float THERMISTOR_NOMINAL_C = 25.0f;
constexpr float THERMISTOR_BETA = RSVP_THERMISTOR_BETA;
constexpr bool THERMISTOR_NTC_TO_GND = (RSVP_THERMISTOR_NTC_TO_GND) != 0;

#if CONFIG_IDF_TARGET_ESP32C6

// Waveshare ESP32-C6-Touch-LCD-1.47 (JD9853 SPI panel + AXS5106L touch).
// No PSRAM, no native USB-OTG, no TCA9554 expander.

constexpr int PIN_BOOT_BUTTON = 9;   // BOOT
constexpr int PIN_PWR_BUTTON = -1;   // No dedicated power button on this board.
// Battery monitoring: the Waveshare board has a built-in 1:2 (i.e. /3) divider
// from the LiPo + rail to GPIO0. So Vbat = ADC_pin_voltage * 3.
constexpr int PIN_BATTERY_ADC = 0;
constexpr float BATTERY_DIVIDER_RATIO = 3.0f;

// Optional external NTC: enable with `-DRSVP_THERMISTOR_PIN=6` for the
// LP803448 LiPo's Yellow lead. Wiring:
//   3V3 ── 10k pull-up ──┬── GPIO6 (ADC1_CH6)
//                        └── Yellow (NTC) ── BAT-/GND
// Black goes to the board's BAT- pad.

constexpr int PIN_LCD_CS = 14;
constexpr int PIN_LCD_DC = 15;
constexpr int PIN_LCD_SCLK = 1;
constexpr int PIN_LCD_MOSI = 2;
constexpr int PIN_LCD_RST = 22;
constexpr int PIN_LCD_BACKLIGHT = 23;
constexpr bool LCD_BACKLIGHT_ACTIVE_LOW = false;

// Legacy QSPI pin names kept as aliases so axs15231b code still compiles when
// the C6 driver picks them up (they are unused for the JD9853 path).
constexpr int PIN_LCD_DATA0 = PIN_LCD_MOSI;
constexpr int PIN_LCD_DATA1 = -1;
constexpr int PIN_LCD_DATA2 = -1;
constexpr int PIN_LCD_DATA3 = -1;

constexpr int PANEL_NATIVE_WIDTH = 172;
constexpr int PANEL_NATIVE_HEIGHT = 320;
constexpr int DISPLAY_WIDTH = 320;
constexpr int DISPLAY_HEIGHT = 172;
constexpr bool UI_ROTATED_180 = false;

// TF (microSD) slot on Waveshare ESP32-C6-Touch-LCD-1.47 wired in SPI mode.
// Confirmed against the official schematic: CLK=IO1, MOSI=IO2, MISO=IO3, CS=IO4.
// CLK and MOSI are intentionally shared with the LCD bus (LCD_SCLK/LCD_MOSI),
// so both peripherals must share Arduino's `SPI` instance (SPI2_HOST/FSPI).
constexpr int PIN_SD_CLK = 1;
constexpr int PIN_SD_CMD = 2;  // MOSI in SPI mode
constexpr int PIN_SD_D0 = 3;   // MISO in SPI mode
constexpr int PIN_SD_CS = 4;

constexpr int PIN_I2C_SDA = 18;
constexpr int PIN_I2C_SCL = 19;
constexpr int PIN_TOUCH_SDA = 18;
constexpr int PIN_TOUCH_SCL = 19;
constexpr int PIN_TOUCH_INT = 21;
constexpr int PIN_TOUCH_RST = 20;

// No GPIO expander on this board.
constexpr int TCA9554_ADDRESS = -1;
constexpr uint8_t TCA9554_PIN_BATTERY_ADC_ENABLE = 0;
constexpr uint8_t TCA9554_PIN_SYS_EN = 0;

#else  // ESP32-S3 (Waveshare ESP32-S3-Touch-LCD-3.49)

constexpr int PIN_BOOT_BUTTON = 0;
constexpr int PIN_PWR_BUTTON = 16;
constexpr int PIN_BATTERY_ADC = 4;

constexpr int PIN_LCD_CS = 9;
constexpr int PIN_LCD_SCLK = 10;
constexpr int PIN_LCD_DATA0 = 11;
constexpr int PIN_LCD_DATA1 = 12;
constexpr int PIN_LCD_DATA2 = 13;
constexpr int PIN_LCD_DATA3 = 14;
constexpr int PIN_LCD_RST = 21;
constexpr int PIN_LCD_BACKLIGHT = 8;
constexpr bool LCD_BACKLIGHT_ACTIVE_LOW = true;

constexpr int PANEL_NATIVE_WIDTH = 172;
constexpr int PANEL_NATIVE_HEIGHT = 640;
constexpr int DISPLAY_WIDTH = 640;
constexpr int DISPLAY_HEIGHT = 172;
constexpr bool UI_ROTATED_180 = true;  // Keep BOOT/PWR at the top edge in landscape.

constexpr int PIN_SD_CLK = 41;
constexpr int PIN_SD_CMD = 39;
constexpr int PIN_SD_D0 = 40;
constexpr int PIN_SD_CS = -1;
constexpr int PIN_I2C_SDA = 47;
constexpr int PIN_I2C_SCL = 48;
constexpr int PIN_TOUCH_SDA = 17;
constexpr int PIN_TOUCH_SCL = 18;
constexpr int PIN_TOUCH_INT = -1;
constexpr int PIN_TOUCH_RST = -1;

constexpr int TCA9554_ADDRESS = 0x20;
constexpr uint8_t TCA9554_PIN_BATTERY_ADC_ENABLE = 1;
constexpr uint8_t TCA9554_PIN_SYS_EN = 6;

#endif

struct BatteryStatus {
  bool present = false;
  float voltage = 0.0f;
  uint8_t percent = 0;
  bool temperatureValid = false;
  float temperatureC = 0.0f;  // battery pack temperature from external NTC, NaN if unread
};

void begin();
void lightSleepUntilBootButton();
bool readBatteryStatus(BatteryStatus &status);
bool releaseBatteryPowerHold();

}  // namespace BoardConfig
