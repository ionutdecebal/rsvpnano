#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_REV2)
constexpr const char *BOARD_ID = "waveshare_esp32s3_touch_lcd_3_49_rev2";
constexpr const char *BOARD_LABEL = "Waveshare ESP32-S3-Touch-LCD-3.49 rev2";
constexpr const char *OTA_ASSET_NAME = "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin";
#else
constexpr const char *BOARD_ID = "waveshare_esp32s3_touch_lcd_3_49";
constexpr const char *BOARD_LABEL = "Waveshare ESP32-S3-Touch-LCD-3.49 rev1";
constexpr const char *OTA_ASSET_NAME = "rsvp-nano-ota.bin";
#endif
constexpr DisplayDriverKind DISPLAY_DRIVER = DisplayDriverKind::Axs15231b;
constexpr TouchControllerKind TOUCH_CONTROLLER = TouchControllerKind::Axs15231b;
constexpr StorageBusKind STORAGE_BUS = StorageBusKind::SdMmc1Bit;
constexpr PowerManagerKind POWER_MANAGER = PowerManagerKind::Tca9554;
constexpr bool HAS_LCD_BACKLIGHT = true;
constexpr bool HAS_AUDIO_OUTPUT = true;
constexpr bool TOUCH_USES_WIRE1 = false;
constexpr bool HAS_IMU = true;
constexpr bool IMU_USES_WIRE1 = true;
constexpr uint8_t IMU_I2C_ADDRESS = 0x6B;
constexpr bool SWAP_APP_BOOT_AND_POWER_BUTTONS = false;
constexpr bool APP_POWER_BUTTON_USES_PMU_EVENTS = false;
constexpr bool POWER_BUTTON_SHORT_TOGGLES_STANDBY = false;
constexpr bool ENABLE_STANDBY_BUTTON_COMBO = true;
constexpr bool BOOT_BUTTON_WAKES_STANDBY = true;
constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = true;
constexpr bool FIRMWARE_POWER_BUTTON_ENABLED = true;
constexpr bool BOOT_BUTTON_TOGGLES_READER = true;
constexpr bool BOOT_BUTTON_BACKS_OUT_OF_MENU = true;
constexpr bool BOOT_BUTTON_TRIPLE_STARTS_STANDBY = false;
constexpr bool BOOT_BUTTON_HOLD_STARTS_STANDBY = true;
constexpr bool TOUCH_READER_PLAYBACK_ENABLED = true;
constexpr bool ENABLE_RESTRUCTURED_MENU = true;

constexpr int PIN_BOOT_BUTTON = 0;
constexpr int PIN_PWR_BUTTON = 16;
constexpr int PIN_KEY_BUTTON = -1;
constexpr int PIN_BATTERY_ADC = 4;
constexpr int PIN_BATTERY_HOLD = -1;

constexpr int PIN_LCD_CS = 9;
constexpr int PIN_LCD_SCLK = 10;
constexpr int PIN_LCD_DATA0 = 11;
constexpr int PIN_LCD_DATA1 = 12;
constexpr int PIN_LCD_DATA2 = 13;
constexpr int PIN_LCD_DATA3 = 14;
constexpr int PIN_LCD_RST = 21;
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_REV2)
constexpr int PIN_LCD_BACKLIGHT = 42;
#else
constexpr int PIN_LCD_BACKLIGHT = 8;
#endif

constexpr int PANEL_NATIVE_WIDTH = 172;
constexpr int PANEL_NATIVE_HEIGHT = 640;
constexpr int DISPLAY_WIDTH = 640;
constexpr int DISPLAY_HEIGHT = 172;
constexpr int READER_CHROME_MARGIN_X = 12;
constexpr int READER_CHROME_MARGIN_TOP = 8;
constexpr int READER_CHROME_MARGIN_BOTTOM = 8;
constexpr int READER_BATTERY_MARGIN_X = READER_CHROME_MARGIN_X;
constexpr int READER_BATTERY_MARGIN_TOP = READER_CHROME_MARGIN_TOP;
constexpr int PIN_DEEP_SLEEP_WAKE = PIN_PWR_BUTTON;
constexpr bool SUPPORTS_SOFTWARE_POWEROFF = true;
constexpr bool RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP = true;
constexpr bool REQUEST_PMU_SHUTDOWN_ON_POWEROFF = false;
constexpr bool PMU_REQUIRES_POWER_KEY_CONFIG = false;
constexpr uint8_t PMU_POWER_KEY_ON_TIME_VALUE = 0x00;
constexpr uint8_t PMU_POWER_KEY_OFF_TIME_VALUE = 0x00;
constexpr uint32_t PMU_BOOT_BUTTON_IGNORE_MS = 1200;
constexpr uint32_t SOFT_OFF_WAKE_CONFIRM_MS = 90;
constexpr size_t DISPLAY_TX_CHUNK_BYTES = 16 * 1024;
constexpr bool UI_ROTATED_180 = true;  // Keep BOOT/PWR at the top edge in landscape.
constexpr UiOrientation DEFAULT_UI_ORIENTATION =
    UI_ROTATED_180 ? UiOrientation::LandscapeFlipped : UiOrientation::Landscape;
constexpr UiOrientation ROTATED_UI_ORIENTATION =
    UI_ROTATED_180 ? UiOrientation::Landscape : UiOrientation::LandscapeFlipped;

constexpr int PIN_SD_CLK = 41;
constexpr int PIN_SD_CMD = 39;
constexpr int PIN_SD_D0 = 40;
constexpr int PIN_I2C_SDA = 47;
constexpr int PIN_I2C_SCL = 48;
constexpr int PIN_TOUCH_SDA = 17;
constexpr int PIN_TOUCH_SCL = 18;
constexpr int PIN_TOUCH_IRQ = -1;
constexpr int PIN_TOUCH_RST = -1;
constexpr uint8_t TOUCH_I2C_ADDRESS = 0x3B;
constexpr bool TOUCH_REQUIRES_MONITOR_MODE = false;
constexpr uint8_t TOUCH_MONITOR_MODE_REGISTER = 0x00;
constexpr uint8_t TOUCH_MONITOR_MODE_VALUE = 0x00;
constexpr uint32_t TOUCH_POLL_INTERVAL_MS = 20;
constexpr uint32_t TOUCH_FAILURE_BACKOFF_MS = 250;
constexpr uint32_t TOUCH_RECOVERY_RETRY_MS = 1000;
constexpr uint32_t TOUCH_RECOVERY_EVENT_IGNORE_MS = 0;

constexpr int TCA9554_ADDRESS = 0x20;
constexpr uint8_t TCA9554_PIN_BACKLIGHT_ENABLE = 1;
constexpr uint8_t TCA9554_PIN_SYS_EN = 6;
constexpr uint8_t TCA9554_PIN_AUDIO_ENABLE = 7;

constexpr int PIN_AUDIO_MCLK = 7;
constexpr int PIN_AUDIO_BCLK = 15;
constexpr int PIN_AUDIO_WS = 46;
constexpr int PIN_AUDIO_DIN = 6;
constexpr int PIN_AUDIO_DOUT = 45;
constexpr uint8_t ES8311_ADDRESS = 0x18;
