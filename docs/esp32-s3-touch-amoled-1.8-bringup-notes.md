# ESP32-S3 Touch AMOLED 1.8 Bring-Up Notes

## Target

- PlatformIO env: `waveshare_esp32s3_touch_amoled_18`
- Platform folder: `src/platforms/waveshare_amoled_18`
- Private board facts: `src/platforms/waveshare_amoled_18/WaveshareAmoled18.h`
- Display driver: `src/drivers/display/sh8601`
- Touch driver: `src/drivers/touch/ft6336`
- Power driver: `src/drivers/power/axp2101`
- GPIO expander driver: `src/drivers/gpio/tca9554`
- IMU driver: `src/drivers/imu/qmi8658`

## Hardware Mapping

- SoC: `ESP32-S3R8`
- Display: `SH8601`
- Native panel geometry: `368x448`
- App/UI geometry: `448x368` landscape
- Touch: `FT3168` routed through the FT6336-compatible driver at I2C `0x38`
- IMU: `QMI8658` at I2C `0x6B`
- PMU: `AXP2101`
- GPIO expander: `TCA9554` at I2C `0x20`
- SDMMC 1-bit: `CLK=2`, `CMD=1`, `D0=3`
- Shared I2C: `SDA=15`, `SCL=14`
- Display QSPI: `CS=12`, `SCLK=11`, `D0..D3=4,5,6,7`

## Current Implementation Shape

The platform implementation exposes only the stable `Board::*` API. Board wiring and chip-specific
facts stay in `WaveshareAmoled18.h`; driver checks stay inside the driver modules.

- `BoardDisplay.cpp` sequences the expander-controlled display/touch rails and calls the SH8601 driver.
- `BoardInput.cpp` reads raw logical controls and raw touch contacts; debouncing and gestures live in `src/input/Input.cpp`.
- `BoardPower.cpp` owns AXP2101 battery and soft-off behavior.
- `BoardStorage.cpp` owns SD bus setup and card-frequency probing.
- `BoardImu.cpp` binds the QMI8658 driver to the shared I2C bus.
- `BoardAudio.cpp` uses the shared ES8311 board-audio helper.

## Input Behavior

The app receives logical input events, not board-specific button flags.

- Physical `BOOT` maps to `InputPrimary`.
- Runtime `PWR` maps to `InputPower` through the board input implementation.
- Touch maps to `InputTouch` with position and gesture data.

Current app behavior:

- `PWR` short press from reader states opens the menu.
- `PWR` short press in menus selects.
- `PWR` hold exits Companion Sync and USB Transfer.
- `BOOT` short press in menus goes back.
- `BOOT` short press while reading is playing or paused cycles theme.
- `BOOT` short press from other reader states cycles brightness.
- `BOOT` hold or triple press enters standby from standby-capable states.
- Standby wakes from logical button or touch events after the grace period.

The old `PWR` + `BOOT` standby combo and board-config button-policy flags are no longer used.

## Board Notes

- Bring-up follows Waveshare's demos by pulsing expander pins `0`, `1`, and `2` low then high.
- The SD demo drives expander pin `7` high before mounting the card, so board init keeps that pin high.
- The FT3168 path applies the monitor-mode write through the touch driver.
- Touch polling uses the shared input module's recovery and backoff logic.
- The IMU, touch, PMU, and expander share the same `Wire` bus.
- The reader chrome keeps conservative safe margins for the small rounded panel.

## Hardware Test Checklist

- Display orientation and color correctness.
- Touch detection, alignment, edge gestures, and recovery after failed reads.
- BOOT and PWR logical event behavior.
- SD mount, index creation, and browse flow.
- Battery reporting and soft-off wake behavior.
- Audio beep output.

## Current Verification

`waveshare_esp32s3_touch_amoled_18` built successfully during the board/input refactor readiness
check. Hardware behavior still needs manual validation on the physical board.
