# ESP32-S3 Touch AMOLED 2.16 Bring-Up Notes

This first-pass port targets the Waveshare `ESP32-S3-Touch-AMOLED-2.16` on the
`v0.0.5` codebase.

## Board Summary

- MCU: `ESP32-S3R8`
- Display: `CO5300`, `480x480`, QSPI
- Touch: `CST92xx` / `CST9220` family on I2C address `0x5A`
- PMU: `AXP2101`
- Storage: `SD_MMC` 1-bit

## Pins Used

- LCD `CS=12`
- LCD `SCLK=38`
- LCD `DATA0=4`
- LCD `DATA1=5`
- LCD `DATA2=6`
- LCD `DATA3=7`
- LCD `RST=39`
- Touch and PMU `SDA=15`
- Touch and PMU `SCL=14`
- Touch `RST=40`
- Touch `INT=11`
- SD `CLK=2`
- SD `CMD=1`
- SD `D0=3`
- `BOOT` button `GPIO0`
- `KEY` button `GPIO18`
- `PWR` button via `AXP2101` PMU power key, not a dedicated ESP32 GPIO

## Current Port Shape

- Added build target `waveshare_esp32s3_touch_amoled_216`
- Added board profile `WaveshareEsp32S3TouchAmoled216Profile.h`
- Added `CO5300` display backend in `src/display/co5300.cpp`
- Added `CST92xx` touch support in `src/input/TouchHandler.cpp`
- Added `AXP2101` battery and shutdown handling in `src/board/BoardConfig.cpp`

## Intentional First-Pass Limitations

- Audio is disabled for now with `HAS_AUDIO_OUTPUT = false`
- Touch and PMU share the same `Wire` bus, so touch teardown no longer calls `Wire.end()`
- The PMU power key is polled from `AXP2101` IRQ status rather than read from an ESP32 pin

## Display Notes

- The board keeps a `32KB` display transfer chunk size for DMA safety, and the `CO5300` driver rounds each flush down to whole `480px` rows to avoid fixed seam artifacts
- The init sequence is based on Waveshare's official ESP-IDF BSP for the `CO5300`
- The panel is currently kept close to native memory orientation and the shared UI mapping layer handles presentation
- Because the panel is square, orientation tuning should be straightforward on hardware if needed
- The first hardware issue was fixed-position green vertical seams; the current test fix keeps QSPI flushes aligned to whole `480px` rows instead of arbitrary chunk boundaries
- The board now uses reader chrome safe margins to stay inside the rounded-screen mask:
  - side inset `48px`
  - top inset `24px`
  - bottom inset `24px`
- The battery badge, previous-sentence hint, footer labels, and their touch hitboxes all use the same safe-margin model

## Touch Notes

- The CST9217 read command is `0xD000`
- The current port follows Waveshare's ESP-IDF `esp_lcd_touch_cst9217` driver shape: read a 10-byte packet for one touch point and validate packet byte `6` against `0xAB`
- The host does not write an `0xAB` ack back after each read; the earlier CST92xx pass did this and could leave the controller silent
- Touch reads are polled directly instead of gated by the `INT` pin level, because Waveshare's driver uses the interrupt pin as an edge source rather than a required level gate
- CST9217 idle packets should not be treated as hard read failures; failed I2C reads still use retry/backoff/recovery
- Coordinate parsing follows Waveshare's SensorLib:
  - `x = (data[1] << 4) | (data[3] >> 4)`
  - `y = (data[2] << 4) | (data[3] & 0x0F)`
- Valid press event is `0x06`

## PMU Notes

- Battery voltage reads come from `AXP2101` ADC result registers `0x34/0x35`
- Battery percent is read from register `0xA4` when valid
- The `AXP2101` remains the battery/PMU device, but app power-off currently uses recoverable soft-off instead of true PMU shutdown
- Power-button held state is synthesized from `AXP2101` power-key IRQ status in `INTSTS2` after enabling the matching IRQ bits in `INTEN2`
- Power-off is board-specific:
  - app `OFF` saves state, blanks/sleeps the display, ends storage/touch, skips AXP2101 shutdown, and waits for `PWR` or `BOOT`
  - this mirrors the stabilized `1.8` behavior and avoids relying on full PMU-off wake until that path is proven reliable without USB serial monitor side effects
  - the soft-off release wait is capped at `1200ms`, then wake is armed from either `PWR` or direct `BOOT`
  - unlike the `1.8`, the `2.16` keeps both `PWR` and `BOOT` enabled as soft-off wake sources because hardware testing confirmed the combined menu/power/standby mapping is stable

## Button Mapping Notes

- `PWR` tap opens/closes the main menu and backs out of submenus
- `PWR` hold powers off from the reader/menu, or exits full-screen utility pages
- `BOOT` short press cycles brightness
- `BOOT` hold cycles theme
- `KEY` short press toggles reader play/pause, using the configured pause mode
- `KEY` hold starts standby/screensaver
- Swipe down from the top edge opens/closes the main menu
- Swipe up from the bottom edge opens quick settings for brightness, theme, focus timer, and sync
- `PWR` uses the PMU-backed power button behavior instead of a GPIO fallback

## Build Verification

Verified locally:

- `waveshare_esp32s3_touch_amoled_216`
- `waveshare_esp32s3_touch_amoled_241`
- `waveshare_esp32s3`
- `waveshare_esp32s3_rev2`

## Expected First Hardware Checks

- Display orientation and color correctness
- Touch alignment and axis direction
- SD card mounting
- Battery reporting
- PMU shutdown behavior

## Source References

- Waveshare ESP-IDF BSP:
  - `examples/ESP-IDF-v5.5/04_Immersive_block/components/esp32_s3_touch_amoled_2_16`
- Waveshare SensorLib:
  - `examples/Arduino-v3.3.5/libraries/SensorLib/src/touch/TouchDrvCST92xx.cpp`
- Waveshare XPowersLib:
  - `examples/ESP-IDF-v5.5/01_AXP2101/components/XPowersLib`
