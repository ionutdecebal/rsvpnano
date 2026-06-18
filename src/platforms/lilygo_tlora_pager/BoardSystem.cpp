#include "board/BoardDisplay.h"
#include "board/BoardPower.h"
#include "board/BoardSystem.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager system backend. LilyGoLib owns the buses, expander, power
// rails and panel, so most of the direct-GPIO bring-up the Waveshare boards do
// is replaced by a single idempotent ensureBegun(). Only the BOOT button (GPIO0)
// is configured here for normal reads and wake-from-sleep.

namespace Board {

namespace System {

void begin() {
  // Full BSP init must run before any Board:: backend touches a peripheral.
  tpager::ensureBegun();

  if (Config::PIN_PWR_BUTTON >= 0) {
    pinMode(Config::PIN_PWR_BUTTON, INPUT_PULLUP);
  }

  Board::Power::begin();
}

void lightSleepUntilBootButton() {
  const int wakePin = Config::PIN_DEEP_SLEEP_WAKE >= 0 ? Config::PIN_DEEP_SLEEP_WAKE
                                                       : Config::PIN_PWR_BUTTON;
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

const char *wakeLabel(bool) { return "Press BOOT to start"; }

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
    case ESP_RST_DEEPSLEEP:
      return "deep_sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown";
  }
}

const char *wakeupCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "ext0";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "gpio";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
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
}

}  // namespace System

}  // namespace Board
