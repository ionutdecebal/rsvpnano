# ESP32-S3 Touch AMOLED 1.8 Bring-Up Notes

## Target

- PlatformIO envs: `waveshare_esp32s3_touch_amoled_18_v1`,
  `waveshare_esp32s3_touch_amoled_18_v2`
- Platform folder: `src/platforms/waveshare_amoled_18`
- Private board facts: `src/platforms/waveshare_amoled_18/WaveshareAmoled18.h`
- Version facts: `src/platforms/waveshare_amoled_18/v1/WaveshareAmoled18Version.h`,
  `src/platforms/waveshare_amoled_18/v2/WaveshareAmoled18Version.h`
- v1 display/touch: `src/drivers/display/sh8601`, `src/drivers/touch/ft6336`
- v2 display/touch: `src/drivers/display/co5300`, `src/drivers/touch/cst92xx`
- Power driver: `src/drivers/power/axp2101`
- GPIO expander driver: `src/drivers/gpio/tca9554`
- IMU driver: `src/drivers/imu/qmi8658`

## Hardware Mapping

- SoC: `ESP32-S3R8`
- v1 display: `SH8601`
- v2 display: `CO5300`
- Native panel geometry: `368x448`
- v2 CO5300 column offset: `16px`, matching Waveshare's Arduino CO5300 constructor.
- App/UI geometry: `448x368` landscape
- v1 touch: `FT3168` routed through the FT6336-compatible driver at I2C `0x38`
- v2 touch: CST92xx-compatible touch at I2C `0x15`
- IMU: `QMI8658` at I2C `0x6B`
- PMU: `AXP2101`
- GPIO expander: `TCA9554` at I2C `0x20`
- SDMMC 1-bit: `CLK=2`, `CMD=1`, `D0=3`
- Shared I2C: `SDA=15`, `SCL=14`
- Display QSPI: `CS=12`, `SCLK=11`, `D0..D3=4,5,6,7`

## Current Implementation Shape

The platform implementation exposes only the stable `Board::*` API. Board wiring and chip-specific
facts stay in `WaveshareAmoled18.h` and the selected `v1`/`v2` version header; driver checks stay
inside the driver modules.

- `v1/BoardDisplay.cpp` binds the SH8601 driver.
- `v2/BoardDisplay.cpp` binds the CO5300 driver.
- `v1/BoardInput.cpp` reads FT6336-compatible touch contacts.
- `v2/BoardInput.cpp` reads CST92xx-compatible touch contacts.
- Shared input debouncing and gestures live in `src/input/Input.cpp`.
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
- The v2 CO5300 path keeps panel-memory rotation as a version fact. PR #116 showed that public
  `PANEL_FLIP_180`-style flags make shared App/Input/Display code care about board-specific panel
  mounting; this implementation keeps the fix local to `v2/WaveshareAmoled18Version.h`.
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

`waveshare_esp32s3_touch_amoled_18_v1` and `waveshare_esp32s3_touch_amoled_18_v2` build
successfully after the version split. Hardware behavior still needs manual validation on the
physical boards.
