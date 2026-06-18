#include "board/BoardDisplay.h"
#include "board/BoardPower.h"
#include "board/BoardSystem.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

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

namespace System {

void begin() {
  if constexpr (WaveshareAmoled206::Buttons::kBootPin >= 0) {
    pinMode(WaveshareAmoled206::Buttons::kBootPin, INPUT_PULLUP);
  }
  if constexpr (WaveshareAmoled206::System::kTouchIrqPin >= 0) {
    pinMode(WaveshareAmoled206::System::kTouchIrqPin, INPUT_PULLUP);
  }
  beginWire(Wire, WaveshareAmoled206::System::kTouchSdaPin,
            WaveshareAmoled206::System::kTouchSclPin,
            WaveshareAmoled206::System::kTouchI2cClockHz,
            WaveshareAmoled206::System::kTouchI2cTimeoutMs);

  Board::Power::begin();
  Board::Display::enablePowerIfAvailable();
}

void lightSleepUntilBootButton() {
  if constexpr (WaveshareAmoled206::System::kDeepSleepWakePin < 0) {
    return;
  }

  constexpr int wakePin = WaveshareAmoled206::System::kDeepSleepWakePin;
  pinMode(wakePin, INPUT_PULLUP);
  gpio_wakeup_enable(WaveshareAmoled206::System::kDeepSleepWakeGpio, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  Serial.flush();
  esp_light_sleep_start();
  gpio_wakeup_disable(WaveshareAmoled206::System::kDeepSleepWakeGpio);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

void holdBacklightOffForDeepSleep() {
  Board::Power::prepareDeepSleepPowerHold();
  Board::Display::holdBacklightOffForDeepSleep();
}

void resetWakePeripherals() { Board::Power::resetWakePeripherals(); }

void deepSleepUntilConfiguredWake() {
  constexpr int wakePin = WaveshareAmoled206::System::kDeepSleepWakePin;
  pinMode(wakePin, INPUT_PULLUP);
  const uint32_t waitStartMs = millis();
  while (!digitalRead(wakePin) && millis() - waitStartMs < 1000) {
    delay(10);
  }
  esp_sleep_enable_ext0_wakeup(WaveshareAmoled206::System::kDeepSleepWakeGpio, 0);
  esp_deep_sleep_start();
}

const char *wakeLabel(bool useRecoverableSoftOff, bool externalPowerPresent) {
  if (useRecoverableSoftOff) {
    return "Press PWR/BOOT to wake";
  }
  return externalPowerPresent ? "Press PWR to wake" : "Press PWR to start";
}

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
