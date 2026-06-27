#include "board/BoardDisplay.h"
#include "board/BoardPower.h"
#include "board/BoardSystem.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <esp_sleep.h>

namespace Board {

namespace {

void beginWire(TwoWire &wire, int sda, int scl, uint32_t clockHz, uint32_t timeoutMs) {
  if (sda < 0 || scl < 0) {
    return;
  }

  wire.begin(sda, scl);
  wire.setClock(clockHz);
  wire.setTimeOut(timeoutMs);
}

void pulseDirectTouchResetPin(uint32_t lowDelayMs, uint32_t highDelayMs) {
  if (Config::PIN_TOUCH_RST < 0) {
    return;
  }

  pinMode(Config::PIN_TOUCH_RST, OUTPUT);
  digitalWrite(Config::PIN_TOUCH_RST, LOW);
  delay(lowDelayMs);
  digitalWrite(Config::PIN_TOUCH_RST, HIGH);
  delay(highDelayMs);
}

}  // namespace

namespace System {

void begin() {
  if (Config::PIN_BOOT_BUTTON >= 0) {
    pinMode(Config::PIN_BOOT_BUTTON, INPUT_PULLUP);
  }
  if (Config::PIN_PWR_BUTTON >= 0) {
    pinMode(Config::PIN_PWR_BUTTON, INPUT_PULLUP);
  }
  if (Config::PIN_TOUCH_IRQ >= 0) {
    pinMode(Config::PIN_TOUCH_IRQ, INPUT_PULLUP);
  }
  if (Config::HAS_LCD_BACKLIGHT && Config::PIN_LCD_BACKLIGHT >= 0) {
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(static_cast<gpio_num_t>(Config::PIN_LCD_BACKLIGHT));
    pinMode(Config::PIN_LCD_BACKLIGHT, OUTPUT);
    digitalWrite(Config::PIN_LCD_BACKLIGHT, LOW);
  }

  if (Config::TOUCH_USES_WIRE1) {
    beginWire(Wire1, Config::PIN_TOUCH_SDA, Config::PIN_TOUCH_SCL, Config::TOUCH_I2C_CLOCK_HZ,
              Config::TOUCH_I2C_TIMEOUT_MS);
  } else {
    beginWire(Wire, Config::PIN_TOUCH_SDA, Config::PIN_TOUCH_SCL, Config::TOUCH_I2C_CLOCK_HZ,
              Config::TOUCH_I2C_TIMEOUT_MS);
  }

  if (!Config::TOUCH_USES_WIRE1 && Config::PIN_I2C_SDA >= 0 && Config::PIN_I2C_SCL >= 0 &&
      (Config::PIN_I2C_SDA != Config::PIN_TOUCH_SDA ||
       Config::PIN_I2C_SCL != Config::PIN_TOUCH_SCL)) {
    beginWire(Wire1, Config::PIN_I2C_SDA, Config::PIN_I2C_SCL, Config::SYSTEM_I2C_CLOCK_HZ,
              Config::SYSTEM_I2C_TIMEOUT_MS);
  }

  Board::Power::begin();
  Board::Display::enablePowerIfAvailable();
}

void lightSleepUntilBootButton() {
  const int wakePin =
      Config::PIN_DEEP_SLEEP_WAKE >= 0
          ? Config::PIN_DEEP_SLEEP_WAKE
          : (Config::PIN_BOOT_BUTTON >= 0 ? Config::PIN_BOOT_BUTTON : Config::PIN_PWR_BUTTON);
  if (wakePin < 0) {
    return;
  }

  pinMode(wakePin, INPUT_PULLUP);
  gpio_wakeup_enable(static_cast<gpio_num_t>(wakePin), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  Serial.flush();
  esp_light_sleep_start();
  gpio_wakeup_disable(static_cast<gpio_num_t>(wakePin));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

void holdBacklightOffForDeepSleep() {
  Board::Power::prepareDeepSleepPowerHold();
  Board::Display::holdBacklightOffForDeepSleep();
}

void resetWakePeripherals() { Board::Power::resetWakePeripherals(); }

void resetTouchController() { pulseDirectTouchResetPin(12, 12); }

void deepSleepUntilConfiguredWake() {
  const int wakePin = Config::PIN_DEEP_SLEEP_WAKE;
  if (wakePin >= 0) {
    pinMode(wakePin, INPUT_PULLUP);
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(wakePin), 0);
  }
  esp_deep_sleep_start();
}

namespace {

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:
      return "task_watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep_sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown";
  }
}

const char *wakeupCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "ext0";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
      return "ulp";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "gpio";
    case ESP_SLEEP_WAKEUP_UART:
      return "uart";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return "undefined";
  }
}

}  // namespace

void logStartupDiagnostics() {
  const esp_reset_reason_t resetReason = esp_reset_reason();
  const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  Serial.printf("[diag] reset=%s(%d) sleep_wake=%s(%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason), wakeupCauseName(wakeupCause),
                static_cast<int>(wakeupCause));

  const Board::Power::DiagnosticSnapshot power = Board::Power::diagnosticSnapshot();
  if (!power.available) {
    Serial.println("[diag] power_snapshot=unavailable");
    return;
  }

  Serial.printf("[diag] power_snapshot=vbus:%u axp_status1:0x%02X axp_status2:0x%02X "
                "axp_pwr_irq:0x%02X\n",
                power.externalPowerPresent ? 1 : 0, power.status1, power.status2,
                power.powerKeyIrqStatus);
}

}  // namespace System

}  // namespace Board
