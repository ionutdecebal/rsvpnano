# ESP32-S3 Touch AMOLED 2.06 Bring-Up Notes

## Target

- PlatformIO env: `waveshare_esp32s3_touch_amoled_206`
- Platform folder: `src/platforms/waveshare_amoled_206`
- Private board facts: `src/platforms/waveshare_amoled_206/WaveshareAmoled206.h`
- Display driver: `src/drivers/display/co5300`
- Touch driver: `src/drivers/touch/ft6336`
- Power driver: `src/drivers/power/axp2101`
- IMU driver: `src/drivers/imu/qmi8658`

## Hardware Mapping

- MCU: `ESP32-S3R8`
- Display: `CO5300`, `410x502`, QSPI
- Touch: `FT3168` routed through the FT6336-compatible driver at I2C `0x38`
- PMU: `AXP2101`
- IMU: `QMI8658`
- Audio codec: `ES8311`
- Storage: `SD_MMC` 1-bit
- LCD QSPI: `CS=12`, `SCLK=11`, `DATA0..3=4,5,6,7`, `RST=8`
- Display column offset: `22px`
- Shared touch/PMU/audio/IMU I2C: `SDA=15`, `SCL=14`
- Touch reset/interrupt: `RST=9`, `INT=38`
- SD: `CLK=2`, `CMD=1`, `D0=3`
- Audio: `MCLK=16`, `BCLK=41`, `WS=45`, `DOUT=40`, `DIN=42`, enable `GPIO46`
- Buttons: `BOOT=GPIO0`, `PWR` through the AXP2101 power key

## Current Implementation Shape

The 2.06 implementation follows the same board/API split as the other targets:

- `BoardDisplay.cpp` binds the CO5300 driver with the 2.06 panel geometry and column offset.
- `BoardInput.cpp` exposes raw logical controls and FT3168 touch contacts.
- `Input::poll` owns debouncing, press duration, touch tracking, and gestures.
- `BoardPower.cpp` owns AXP2101 battery/power behavior and the GPIO46 audio rail.
- `BoardStorage.cpp` owns SD bus setup and frequency probing.
- `BoardImu.cpp` binds the QMI8658 driver to the shared I2C bus.
- `BoardAudio.cpp` uses the shared ES8311 board-audio helper.

## Hardware Test Checklist

- Display orientation, color correctness, brightness, sleep, and wake.
- Confirm the `22px` CO5300 column offset on real hardware.
- Touch alignment, tap, drag, edge gestures, and failed-read recovery.
- BOOT and PMU PWR logical events.
- SD card mount, index creation, and browse flow.
- Battery reporting and soft-off wake behavior.
- Audio beep output through ES8311 after enabling GPIO46.

## Source Notes

Waveshare's public wiki lists the 2.06 board as a `410x502` AMOLED with CO5300 display, FT3168 touch,
QMI8658 IMU, PCF85063 RTC, ES8311 audio, AXP2101 power management, two side buttons, and a TF card
slot. The Arduino demo pin header provides the display, touch, and SD pins, while the ES8311 demo
provides the audio pins and GPIO46 enable line.

- Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06
- Demo repository: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06
- Pin header: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/blob/main/examples/Arduino-v3.2.0/libraries/Mylibrary/pin_config.h
- ES8311 demo: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/blob/main/examples/Arduino-v3.2.0/examples/08_ES8311/08_ES8311.ino
