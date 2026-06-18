#pragma once

#include "board/BoardTypes.h"

// ---------------------------------------------------------------------------
// LilyGo T-LoRa-Pager platform config.
//
// ST7796U SPI panel (480x222 landscape), rotary encoder + TCA8418 keyboard
// (no touch panel), XL9555 IO expander, BQ25896 charger + BQ27220 fuel gauge,
// SPI SD card, ES8311 audio + DRV2605 haptics. All peripheral access goes
// through the LilyGoLib BSP (see platforms/lilygo_tlora_pager/TPagerHardware.h);
// the pin constants below are mostly informational because LilyGoLib owns the
// buses. The Board:: backends in this directory are fully custom and route
// every access through LilyGoLib rather than the shared direct-GPIO drivers.
//
// Display geometry: the ST7796 panel is natively landscape (480x222), so the
// renderer uses the identity (Portrait) orientation -- logical canvas maps
// straight onto the panel, never the native-portrait rotation the Waveshare
// boards use. The left-handed / flipped mode is a true 180 deg flip
// (PortraitFlipped).
//
// Buttons: the only user-readable hardware button is the physical BOOT button
// (GPIO0), wired here as PIN_PWR_BUTTON. In App's button model that gives:
//   short press -> open/close menu (toggleMenuFromPowerButton)
//   long press  -> power-off confirm (SUPPORTS_SOFTWARE_POWEROFF)
// The rotary encoder and its center push are synthesized into touch gestures by
// platforms/lilygo_tlora_pager/InputTouch.cpp (rotate = navigation/WPM/scrub,
// center = select / play-pause). There is no second physical button, so
// PIN_BOOT_BUTTON / PIN_KEY_BUTTON are inert (-1).
// ---------------------------------------------------------------------------

namespace Board::Config {

using UiOrientation = Board::UiOrientation;
using BatteryStatus = Board::BatteryStatus;
using PowerDiagnosticSnapshot = Board::PowerDiagnosticSnapshot;

constexpr const char *BOARD_ID = "lilygo_t_lora_pager";
constexpr const char *BOARD_LABEL = "LilyGo T-LoRa-Pager";
constexpr const char *OTA_ASSET_NAME = "rsvp-nano-tpager-ota.bin";

// SD shares the main SPI bus and is mounted by LilyGoLib (installSD()), not the
// SDMMC peripheral. The mount lives in this platform's BoardStorage backend; the
// post-PR#120 Board layer no longer carries a declarative StorageBusKind here.

constexpr bool HAS_LCD_BACKLIGHT = true;   // AW9364 charge-pump, driven by LilyGoLib.
constexpr bool HAS_AUDIO_OUTPUT = true;
constexpr bool TOUCH_USES_WIRE1 = false;
// This board's IMU is a Bosch BHI260AP sensor hub (I2C 0x28), already booted
// and owned by the LilyGoLib BSP -- not a QMI8658 on a raw bus. The platform
// Board::Imu backend (platforms/lilygo_tlora_pager/BoardImu.cpp) reads its
// gravity virtual sensor through the BSP, so the focus timer's movement
// detection works. The Wire/release/address fields below are unused on this
// board (the generic QMI8658 path is excluded); kept for config completeness.
constexpr bool HAS_IMU = true;
constexpr bool IMU_USES_WIRE1 = false;
constexpr bool IMU_RELEASE_BUS_BEFORE_READ = false;
constexpr uint8_t IMU_I2C_ADDRESS = 0x28;

constexpr bool SWAP_APP_BOOT_AND_POWER_BUTTONS = false;
constexpr bool APP_POWER_BUTTON_USES_PMU_EVENTS = false;
constexpr bool BOOT_BUTTON_WAKES_STANDBY = true;
constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
constexpr bool FIRMWARE_POWER_BUTTON_ENABLED = true;
constexpr bool BOOT_BUTTON_TOGGLES_READER = true;
// Touchless board: a reader center tap (synthesized from the rotary push) toggles
// playback, since there is no dedicated KEY/BOOT button to start RSVP. In the
// post-PR#120 Board layer this is the app-facing TOUCH_READER_PLAYBACK_ENABLED
// knob; READER_TAP_TOGGLES_PLAYBACK is kept only for this platform's own
// backends and is otherwise unused by shared code.
constexpr bool READER_TAP_TOGGLES_PLAYBACK = true;
constexpr bool TOUCH_READER_PLAYBACK_ENABLED = true;
// The center tap toggles playback rather than only pausing a locked reader.
constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = false;
constexpr bool BOOT_BUTTON_BACKS_OUT_OF_MENU = true;
constexpr bool BOOT_BUTTON_HOLD_STARTS_STANDBY = true;
// Use the legacy flat menu, not the restructured one. The restructured layout
// relocates Focus Timer and Companion sync into the Quick Settings panel, which
// is only reachable by a bottom-edge touch swipe. This board synthesizes swipes
// from the rotary encoder anchored at screen center (see InputTouch.cpp), so it
// can never trigger that gesture and both entries would be unreachable. The flat
// menu lists them as top-level items, navigable by rotary scroll + center tap.
constexpr bool ENABLE_RESTRUCTURED_MENU = false;

constexpr int PIN_BOOT_BUTTON = -1;  // No second physical button; kept inert.
constexpr int PIN_PWR_BUTTON = 0;    // Physical BOOT button (GPIO0).
constexpr int PIN_KEY_BUTTON = -1;   // No dedicated key button.
constexpr int PIN_BATTERY_ADC = -1;  // Battery read via BQ27220 fuel gauge.
constexpr int PIN_BATTERY_HOLD = -1;

// ST7796 is SPI (not QSPI); informational only -- LilyGoLib drives the panel.
constexpr int PIN_LCD_CS = 38;
constexpr int PIN_LCD_SCLK = 35;
constexpr int PIN_LCD_DATA0 = 34;  // MOSI on the shared SPI bus.
constexpr int PIN_LCD_DATA1 = -1;
constexpr int PIN_LCD_DATA2 = -1;
constexpr int PIN_LCD_DATA3 = -1;
constexpr int PIN_LCD_RST = -1;       // Display RESET is not connected.
constexpr int PIN_LCD_BACKLIGHT = 42;  // AW9364 backlight driver (LilyGoLib-managed).

constexpr int PANEL_NATIVE_WIDTH = 480;
constexpr int PANEL_NATIVE_HEIGHT = 222;
constexpr int DISPLAY_WIDTH = 480;
constexpr int DISPLAY_HEIGHT = 222;
constexpr int READER_CHROME_MARGIN_X = 12;
constexpr int READER_CHROME_MARGIN_TOP = 8;
constexpr int READER_CHROME_MARGIN_BOTTOM = 8;
constexpr int READER_BATTERY_MARGIN_X = READER_CHROME_MARGIN_X;
constexpr int READER_BATTERY_MARGIN_TOP = READER_CHROME_MARGIN_TOP;
constexpr int PIN_DEEP_SLEEP_WAKE = PIN_PWR_BUTTON;  // ext0 wake on the BOOT button.
constexpr bool SUPPORTS_SOFTWARE_POWEROFF = true;
constexpr bool RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP = false;
constexpr bool REQUEST_PMU_SHUTDOWN_ON_POWEROFF = false;
constexpr bool SOFTWARE_POWEROFF_USES_SOFT_LOOP = false;
constexpr bool SOFT_OFF_WAKE_USES_POWER_BUTTON = false;
constexpr bool SOFT_OFF_WAKE_USES_BOOT_BUTTON = false;
constexpr bool PMU_REQUIRES_POWER_KEY_CONFIG = false;
constexpr bool AXP2101_RELEASE_BUS_BEFORE_READ = false;
constexpr bool AXP2101_ENABLE_POWER_KEY_IRQS = false;
constexpr bool TCA9554_HAS_DISPLAY_SEQUENCE = false;
constexpr bool TCA9554_HAS_POWER_BUTTON = false;
constexpr bool TCA9554_RELEASE_BUS_BEFORE_READ = false;
constexpr uint8_t PMU_POWER_KEY_ON_TIME_VALUE = 0x00;
constexpr uint8_t PMU_POWER_KEY_OFF_TIME_VALUE = 0x00;
constexpr uint32_t PMU_BOOT_BUTTON_IGNORE_MS = 1200;
constexpr uint32_t SOFT_OFF_WAKE_CONFIRM_MS = 90;
constexpr uint32_t SYSTEM_I2C_CLOCK_HZ = 300000;
constexpr uint32_t SYSTEM_I2C_TIMEOUT_MS = 10;
constexpr uint32_t TOUCH_I2C_CLOCK_HZ = 300000;
constexpr uint32_t TOUCH_I2C_TIMEOUT_MS = 10;
constexpr size_t DISPLAY_TX_CHUNK_BYTES = 16 * 1024;
constexpr bool UI_ROTATED_180 = false;  // Panel is natively landscape; identity map.
constexpr UiOrientation DEFAULT_UI_ORIENTATION =
    UI_ROTATED_180 ? UiOrientation::PortraitFlipped : UiOrientation::Portrait;
constexpr UiOrientation ROTATED_UI_ORIENTATION =
    UI_ROTATED_180 ? UiOrientation::Portrait : UiOrientation::PortraitFlipped;

// SD shares the main SPI bus; informational only (LilyGoLib::installSD() owns the
// mount, power rail, CS pin and shared-bus settings).
constexpr int PIN_SD_CLK = 35;
constexpr int PIN_SD_CMD = 34;
constexpr int PIN_SD_D0 = 33;
constexpr int PIN_I2C_SDA = 3;
constexpr int PIN_I2C_SCL = 2;

// No touch panel on this board. The constants below exist only so the shared
// touch-config surface compiles; the generic input/InputTouch.cpp is excluded
// for this platform in favour of the rotary-encoder InputTouch.cpp here.
constexpr int PIN_TOUCH_SDA = -1;
constexpr int PIN_TOUCH_SCL = -1;
constexpr int PIN_TOUCH_IRQ = -1;
constexpr int PIN_TOUCH_RST = -1;
constexpr uint8_t TOUCH_I2C_ADDRESS = 0x00;
constexpr bool TOUCH_REQUIRES_MONITOR_MODE = false;
constexpr bool TOUCH_RELEASE_BUS_BEFORE_READ = false;
constexpr uint8_t TOUCH_MONITOR_MODE_REGISTER = 0x00;
constexpr uint8_t TOUCH_MONITOR_MODE_VALUE = 0x00;
constexpr uint32_t TOUCH_POLL_INTERVAL_MS = 20;
constexpr uint32_t TOUCH_FAILURE_BACKOFF_MS = 250;
constexpr uint32_t TOUCH_RECOVERY_RETRY_MS = 1000;
constexpr uint32_t TOUCH_RECOVERY_EVENT_IGNORE_MS = 0;

// XL9555 is the IO expander on this board (not TCA9554). These placeholders keep
// shared code that references the TCA9554 names compiling; the custom Board
// backends never touch them (expander access goes through LilyGoLib).
constexpr int TCA9554_ADDRESS = 0x20;
constexpr uint8_t TCA9554_PIN_BACKLIGHT_ENABLE = 0;
constexpr uint8_t TCA9554_PIN_PWR_BUTTON = 0;
constexpr uint8_t TCA9554_PIN_PMU_IRQ = 0;
constexpr uint8_t TCA9554_PIN_SD_ENABLE = 0;
constexpr uint8_t TCA9554_PIN_TOUCH_RESET = 0;
constexpr uint8_t TCA9554_PIN_LCD_RESET = 0;
constexpr uint8_t TCA9554_PIN_DISPLAY_ENABLE = 0;
constexpr uint8_t TCA9554_PIN_SYS_EN = 0;
constexpr uint8_t TCA9554_PIN_AUDIO_ENABLE = 0;

// ES8311 audio (informational; LilyGoLib's codec owns the I2S/I2C setup).
constexpr int PIN_AUDIO_MCLK = 10;
constexpr int PIN_AUDIO_BCLK = 11;
constexpr int PIN_AUDIO_WS = 18;
constexpr int PIN_AUDIO_DIN = 17;
constexpr int PIN_AUDIO_DOUT = 45;
constexpr uint8_t ES8311_ADDRESS = 0x18;

}  // namespace Board::Config
