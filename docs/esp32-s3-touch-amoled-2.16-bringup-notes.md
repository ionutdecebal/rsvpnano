# ESP32-S3 Touch AMOLED 2.16 Bring-Up Notes

## Target

- PlatformIO env: `waveshare_esp32s3_touch_amoled_216`
- Platform folder: `src/platforms/waveshare_amoled_216`
- Private board facts: `src/platforms/waveshare_amoled_216/WaveshareAmoled216.h`
- Display driver: `src/drivers/display/co5300`
- Touch driver: `src/drivers/touch/cst92xx`
- Power driver: `src/drivers/power/axp2101`
- IMU driver: `src/drivers/imu/qmi8658`

## Hardware Mapping

- MCU: `ESP32-S3R8`
- Display: `CO5300`, `480x480`, QSPI
- Touch: `CST92xx` / `CST9220` family at I2C `0x5A`
- PMU: `AXP2101`
- IMU: `QMI8658`
- Storage: `SD_MMC` 1-bit
- LCD QSPI: `CS=12`, `SCLK=38`, `DATA0..3=4,5,6,7`, `RST=39`
- Shared touch/PMU I2C: `SDA=15`, `SCL=14`
- Touch reset/interrupt: `RST=40`, `INT=11`
- SD: `CLK=2`, `CMD=1`, `D0=3`
- Buttons: `BOOT=GPIO0`, `KEY=GPIO18`, `PWR` through the AXP2101 power key

## Current Implementation Shape

The board implementation binds its private wiring to the stable `Board::*` API:

- `BoardDisplay.cpp` owns panel reset and calls the CO5300 driver.
- `BoardInput.cpp` exposes raw logical controls and raw touch contacts.
- `Input::poll` owns debounce, press duration, triple press, touch tracking, and gestures.
- `BoardPower.cpp` owns AXP2101 battery and recoverable soft-off behavior.
- `BoardStorage.cpp` owns SD bus setup and frequency probing.
- `BoardAudio.cpp` uses the shared ES8311 board-audio helper.

## Display Notes

- The CO5300 driver rounds flushes down to whole `480px` rows for DMA safety and to avoid fixed seam artifacts.
- The panel is square, so the shared UI mapping layer keeps orientation handling simple.
- The board uses safe reader chrome margins for the rounded screen mask.

## Touch Notes

- The CST9217 read command is `0xD000`.
- The driver reads a 10-byte packet for one touch point and validates packet byte `6` against `0xAB`.
- Idle packets are not treated as hard read failures; I2C failures use the shared input recovery/backoff path.
- Touch reads are not gated by the interrupt pin level because the Waveshare driver treats the interrupt as an edge source.

## Input Behavior

- Physical `BOOT` maps to `InputPrimary`.
- Physical `KEY` maps to `InputKey`.
- PMU power-key events map to `InputPower`.
- Touch maps to `InputTouch`.

The app handles those logical events by state:

- `PWR` short press opens/selects menu actions.
- `BOOT` short press backs out of menus or handles reader brightness/theme shortcuts.
- `KEY` short press toggles reader playback where supported by the app state.
- `BOOT` hold or triple press enters standby from standby-capable states.
- Standby wakes from logical button or touch events after the grace period.

The old board-config button-policy flags and the `PWR` + `BOOT` standby combo are no longer used.

## Hardware Test Checklist

- Display orientation and color correctness.
- Touch alignment, movement, tap, edge gestures, and failed-read recovery.
- BOOT, KEY, and PMU PWR logical events.
- SD card mount, index creation, and browse flow.
- Battery reporting and soft-off wake behavior.
- Audio beep output.

## Current Verification

This target was not part of the most recent successful representative build pair. It should be
included in the next full PlatformIO matrix run before merging.
