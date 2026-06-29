# ESP32-S3 Touch AMOLED 2.41 Bring-Up Notes

## Target

- PlatformIO env: `waveshare_esp32s3_touch_amoled_241`
- Platform folder: `src/platforms/waveshare_amoled_241`
- Private board facts: `src/platforms/waveshare_amoled_241/WaveshareAmoled241.h`
- Display driver: `src/drivers/display/rm690b0`
- Touch driver: `src/drivers/touch/ft6336`
- IMU driver: `src/drivers/imu/qmi8658`

## Current Hardware Status

Previously validated behavior on the 2.41 board:

- Display initializes and renders.
- Touch input works and follows display orientation.
- Colors are correct after removing the extra RGB565 byte swap.
- Layout aligns with the visible panel area.
- Fixed on-screen stripe artifacts were resolved by board-specific transfer chunk sizing.
- SD card loading works on the 2.41.
- Battery hold and ADC paths are wired for this board.

Still needs manual validation after the board/input refactor:

- Soft-off and wake behavior on battery.
- USB MSC behavior.
- Long reading session stability.
- OTA asset selection on a real release.

## Hardware Assumptions

- Panel native geometry: `450x600`
- App/UI geometry: `600x450`
- Touch controller: `FT6336`
- Display controller: `RM690B0`
- I2C: `GPIO47/48`
- Touch reset: `GPIO3`
- Battery hold: `GPIO16`
- Battery ADC: `GPIO17`
- SDMMC pins: `GPIO4/5/6`

These facts live in `WaveshareAmoled241.h` and are consumed only by the platform implementation.

## Current Implementation Shape

- `BoardDisplay.cpp` owns board power/rail sequencing and calls the RM690B0 driver.
- `BoardInput.cpp` exposes raw logical controls and FT6336 touch contacts.
- `Input::poll` owns debounce, press duration, touch tracking, and app-facing gestures.
- `BoardPower.cpp` owns battery hold, battery ADC, sleep, wake, and power behavior.
- `BoardStorage.cpp` owns SD bus setup and card-frequency probing.
- `BoardImu.cpp` binds the QMI8658 driver to the board I2C bus.

The shared app does not include platform or chip-driver headers directly.

## Bring-Up Lessons

### Rotation

Keep panel memory in native portrait-style geometry and let the shared display/input mapping layer
perform the landscape presentation. Avoid rotating both in the panel driver and in app mapping.

### Color Format

The app already byte-swaps RGB565 before sending pixels to the panel. The RM690B0 path must not add a
second swap.

### Panel Addressing

The `16px` RM690B0 column offset belongs in panel addressing, not in app layout code.

### Transfer Chunking

The fixed black stripe artifacts were caused by visible seams between display flush bands on the
rotated panel path. The 2.41 target uses a larger board-specific transfer buffer to avoid those seams.

## Input Behavior

The platform maps physical hardware to logical input controls, and the app handles the resulting
events by state. The old board-config button-policy flags and `PWR` + `BOOT` standby combo are no
longer used.

## OTA Separation

The 2.41 release asset name is:

```text
rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin
```

The updater rejects obvious cross-board asset overrides by name. Embedded board metadata would still
be a stronger future guard.

## Hardware Test Checklist

- Display orientation, colors, brightness, sleep, and wake.
- Touch alignment, edge gestures, and failed-read recovery.
- SD load behavior and index creation.
- Battery reporting.
- Soft-off and wake on USB and battery.
- USB transfer mode.
- Long reading session stability.

## Current Verification

This target was not part of the most recent successful representative build pair. It should be
included in the next full PlatformIO matrix run before merging.
