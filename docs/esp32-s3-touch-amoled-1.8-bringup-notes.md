# ESP32-S3-Touch-AMOLED-1.8 Bring-Up Notes

This board was added as a separate target on the `v0.0.5` multi-board integration branch.

## Target

- PlatformIO envs:
  - `waveshare_esp32s3_touch_amoled_18` for V1 boards
  - `waveshare_esp32s3_touch_amoled_18_v2` for V2 test boards
- USB MSC transfer is enabled in this env for Quick Settings `USB Sync`.
- Board profile: `src/board/profiles/WaveshareEsp32S3TouchAmoled18Profile.h`
- Display drivers:
  - V1: `src/drivers/display/sh8601`
  - V2: `src/drivers/display/co5300`

## Hardware mapping used

- SoC: `ESP32-S3R8`
- Display:
  - V1: `SH8601`
  - V2: `CO5300`
- Native panel geometry: `368x448`
- App/UI geometry: `448x368` landscape
- Touch:
  - V1: `FT3168` on I2C `0x38`
  - V2: `CST816` on I2C `0x15`
- IMU: `QMI8658` on shared I2C `0x6B`
- PMU: `AXP2101`
- GPIO expander: `TCA9554` at `0x20`
- SDMMC 1-bit: `CLK=2`, `CMD=1`, `D0=3`
- Shared I2C: `SDA=15`, `SCL=14`
- Display QSPI: `CS=12`, `SCLK=11`, `D0..D3=4,5,6,7`

## V1/V2 difference notes

- Waveshare's official Arduino examples now split this product into `Arduino-v3.3.5` and
  `Arduino-v3.3.5-v2`.
- The exported pin macro file is effectively unchanged between those trees: display QSPI remains
  `CS=12`, `SCLK=11`, `D0..D3=4,5,6,7`; shared I2C remains `SDA=15`, `SCL=14`; `TP_INT`
  remains `GPIO21`.
- The meaningful hardware/backend changes are the display controller and touch controller:
  V1 uses `Arduino_SH8601` plus `Arduino_FT3x68` / `FT3168_DEVICE_ADDRESS` (`0x38`), while
  V2 uses `Arduino_CO5300` plus `Arduino_CST816x` / `CST816T_DEVICE_ADDRESS` (`0x15`).
- The V2 firmware target is intentionally labeled as a test build until it has been confirmed on
  physical V2 hardware.
- The V2 firmware target keeps polling touch instead of depending on `TP_INT`, because CST816
  interrupt pulses can be shorter than this app's normal polling cadence.

## Important board-specific behavior

- The display and touch are not using direct reset GPIOs in this port.
- Bring-up follows Waveshare's demos by pulsing expander pins `0`, `1`, and `2` low then high.
- The SD demo in Waveshare's repo drives expander pin `7` high before mounting the card, so this port keeps that pin high during board init.
- Runtime BOOT handling is mapped to real `GPIO0` and is active-low.
- Runtime PWR handling is mapped to Waveshare's `EXIO4` input on the `TCA9554` expander and is active-high.
- The official Waveshare FAQ says PWR can be read from `EXIO4` while the board is running, while the PMU owns hardware power-on/off from the fully-off state.
- The local Waveshare `12_LVGL_AXP2101_ADC_Data` demo also configures expander pin `4` as an input and reads it directly for the button action.
- The `AXP2101` is still used for battery data and software shutdown, but it is no longer used as the runtime held-state source for the `1.8` PWR button.
- Current `1.8` button model:
  - firmware ignores runtime `PWR` reads because the expander-backed signal can false-trigger
  - `BOOT` short: toggle reader play/pause, using the configured instant or sentence-end pause mode
  - `BOOT` short in menus: back/close
  - `BOOT` short in Wi-Fi Sync: exit sync
  - `BOOT` long: start standby/screensaver
  - `BOOT` from standby: wake the app after the short standby grace period
  - swipe down from the top edge: open/close menu
  - swipe up from the bottom edge: quick settings for brightness, theme, focus timer, and sync
  - quick settings sync opens a Wi-Fi Sync / USB Sync chooser
  - USB Sync exposes the SD card over USB MSC; eject from the host to remount and return to the reader
  - main menu uses the new 1.8 test hierarchy: resume, chapters, books, articles, settings, power off
  - articles contains back, browse articles, and update RSS
  - settings contains display, word pacing, typography tune, Wi-Fi, firmware update, and SD card check
  - Wi-Fi contains a nested network submenu for choose/forget network, plus Auto OTA and OTA Owner
  - touch playback gestures are disabled; `BOOT` owns reader play/pause
  - `PWR` from soft-off: wake the app after a sustained confirmation window
- The old BOOT/PWR swap experiment is no longer active on this board.
- The `1.8` now uses recoverable soft-off for both USB and battery: the app saves state, blanks/sleeps the display, ends storage/touch, skips AXP2101 shutdown, and waits for `PWR`.
- `PWR` soft-off wake currently requires a `500ms` confirmation window to reject short false pulses from the TCA9554-backed runtime input.
- True AXP2101 PMU shutdown is intentionally deferred because hardware wake was reliable while a USB serial monitor was open but unreliable without monitor/CDC side effects.

## Reuse / assumptions

- `FT3168` and the V2 `CST816` are both routed through the existing `Ft6336` packet reader for
  coordinate polling, because their basic finger-count and X/Y coordinate registers line up for
  this app's single-touch needs.
- After controller detection, the port now applies Waveshare's demo init write of `0xA5 = 0x01` so the `FT3168` stays in monitor mode.
- The V2 target applies Waveshare's CST816 interrupt-mode write of `0xFA = 0x40`.
- Repeated touch read failures now trigger automatic re-initialization instead of permanently disabling touch polling.
- Recoverable soft-off wake re-runs the expander-controlled display/touch release sequence and then performs a full SH8601 init, because the lighter wake path could leave touch and SD unavailable after `PWR` wake.
- The AXP2101 power key is explicitly configured for `128ms` power-on and `6s` PMU fallback power-off, matching Waveshare's documented "click to power on / hold >6s to power off" model more closely.
- The app waits for PWR release before entering soft-off so the next press is treated as a fresh wake request.
- The soft-off release wait is capped at `1200ms` on this board so stale expander state cannot create a multi-second dead zone after the screen turns off.
- The soft-off wake loop waits for the enabled wake button to be released quietly for `250ms`, then requires a wake press to remain stable for `500ms`. On the 1.8 the only enabled soft-off wake button is expander-backed `PWR`.
- Serial flushing is skipped in the 1.8 recoverable soft-off path so wake behavior is not affected by whether a USB CDC monitor is open.
- The expander-backed PWR read is debounced and failed I2C reads keep the last stable state. Without this, a transient `Wire requestFrom -1` while PWR was held could look like a fake release and immediately bounce the app back into standby.
- The expander-backed PWR read is throttled to `25ms` instead of being sampled every app loop. Without this, running without a serial monitor could hammer the shared I2C bus far harder than the monitored/debug path.
- The shared 1.8 I2C bus now runs at `200kHz`, matching Waveshare's ESP-IDF sample, and 1.8 register reads use stop-then-read instead of repeated-start after repeated `i2cWriteReadNonStop` failures were seen in hardware logs.
- The focus timer IMU uses the same shared `Wire` bus as touch/PMU on the 1.8; boards with a separate system bus continue using `Wire1`.
- Touch polling on the 1.8 is intentionally slower (`50ms`) with longer recovery backoff so a failing touch controller cannot starve the PWR/PMU path.
- After touch init/recovery the 1.8 ignores touch events for `1200ms`, and reader playback uses double tap or press-and-hold to pause rather than single tap while locked, to reduce false pauses from occasional FT3168 ghost taps.
- Audio is intentionally disabled for the first port pass even though the board has `ES8311` hardware.
- The reader chrome uses a conservative safe area because this panel is smaller and has rounded corners.

## First hardware test checklist

- Confirm orientation
- Confirm color correctness
- Confirm touch detection and alignment
- Confirm BOOT runtime behavior
- Confirm PWR behavior
- Confirm SD card mount and browse flow

## Current status

- `waveshare_esp32s3_touch_amoled_18` builds successfully
- Regression builds also passed for:
  - `waveshare_esp32s3_touch_amoled_216`
  - `waveshare_esp32s3_touch_amoled_241`
  - `waveshare_esp32s3`
