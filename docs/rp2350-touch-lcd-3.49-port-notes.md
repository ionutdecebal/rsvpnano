# RP2350 Touch LCD 3.49 Port Notes

## Why this board is a strong replacement candidate

The Waveshare `RP2350-Touch-LCD-3.49` looks like a very plausible successor to the current
`ESP32-S3-Touch-LCD-3.49` target:

- Same overall `3.49"` form factor.
- Same `172 x 640` panel geometry.
- Same `AXS15231B` display/touch controller family.
- Still includes `TF` storage, battery support, audio codec, IMU, and RTC.

Official references:

- Product page: <https://www.waveshare.com/rp2350-touch-lcd-3.49.htm>
- Wiki: <https://www.waveshare.com/wiki/RP2350-Touch-LCD-3.49>
- Schematic PDF: <https://files.waveshare.com/wiki/RP2350-Touch-LCD-3.49/RP2350-Touch-LCD-3.49.pdf>

## Important reality check

This is not a simple pin remap.

The current firmware is deeply tied to the ESP32-S3 platform:

- Display transport uses ESP-IDF SPI APIs in `src/display/axs15231b.cpp`.
- Storage uses `SD_MMC` in `src/storage/StorageManager.cpp` and `src/storage/EpubConverter.cpp`.
- USB mass storage uses ESP32 TinyUSB + `sdmmc` in `src/usb/UsbMassStorageManager.cpp`.
- Sleep, wake, battery hold, and ADC behavior use ESP32 APIs in `src/board/BoardConfig.cpp`.
- Settings use `Preferences` in `src/app/App.h`.
- OTA and Wi-Fi flows use ESP32 networking in `src/update/OtaUpdater.cpp` and `src/app/App.cpp`.
- Audio uses ESP32 I2S driver APIs in `src/audio/AudioManager.cpp`.

The RP2350 board keeps several peripherals, but it drops the onboard Wi-Fi that the current OTA
and network setup UX depends on.

## What should carry over cleanly

These parts are mostly platform-agnostic and should be reusable with minimal or no logic changes:

- Reader/pacing logic in `src/reader/`.
- Most app state and menu flow in `src/app/`, once Wi-Fi/OTA paths are feature-gated.
- Font assets and word layout logic in `src/display/DisplayManager.cpp`.
- Book parsing and format logic, after the storage backend is replaced.
- Touch packet decoding in `src/input/TouchHandler.cpp`, because the RP2350 board still uses AXS15231B touch over I2C.

## What must be rewritten or abstracted

### 1. Display bring-up

The RP2350 board still uses an `AXS15231B` panel over `QSPI`, so the init sequence may be reusable.
The transport layer is not.

Needed work:

- Replace ESP32 `spi_master` usage in `src/display/axs15231b.cpp`.
- Rework backlight PWM control.
- Reconfirm rotation and coordinate mapping on the new panel wiring.

The schematic clearly shows:

- `AXS15231B` display/touch controller.
- `QSPI` display signals.
- `LCD_RST` present on the board.

## 2. Touch

This looks promising for early bring-up because the board keeps AXS15231B touch over `I2C`.

Needed work:

- Replace only the board pin configuration.
- Keep the existing polling and packet decode logic as the first attempt.
- Ignore touch interrupt wiring at first and stay with polling until basic interaction works.

## 3. Storage

This is one of the biggest platform differences.

Current firmware assumes ESP32 `SD_MMC`:

- mount
- file IO
- card size queries
- EPUB conversion temp files
- USB MSC block access

The RP2350 schematic exposes the TF card with `SD_CS`, `SD_MOSI`, `SD_MISO`, and `SD_CLK`, so the
first RP2350 storage backend should be SPI-based, not `SD_MMC`.

Practical direction:

- Replace `SD_MMC` usage with a backend built on `SdFat` or an RP2350-compatible SD SPI stack.
- Do storage read-only first.
- Add write paths only after book loading is stable.

## 4. Power, battery, and sleep

The current ESP32 board uses a `TCA9554` expander for battery-path control and audio rail enable.
The RP2350 schematic instead exposes direct nets such as `BAT_ADC`, `SYS_OUT`, and `SYS_EN`.

That means `src/board/BoardConfig.cpp` needs a board-specific rewrite for:

- boot/power button handling
- battery measurement
- power hold behavior
- sleep/wake behavior

The current ESP32 light/deep sleep flow should be treated as non-portable.

## 5. Preferences and config persistence

`Preferences` is ESP32-specific.

Needed work:

- Introduce a small settings storage abstraction.
- Back it with LittleFS, flash KV, or a compact config file on RP2350.
- Keep the data model, replace the persistence layer.

## 6. Wi-Fi and OTA

The RP2350 board page lists USB, SPI, I2C, UART, ADC, PWM, and PIO features, but not onboard
Wi-Fi.

That means:

- The current OTA updater cannot be ported directly.
- Wi-Fi network scanning and password entry should be feature-gated off on RP2350 builds.
- The firmware update story likely becomes UF2 drag-and-drop first, USB flashing second.

## 7. USB mass storage

This is possible in principle on RP2350, but it should not be phase 1.

Needed work:

- Rebuild MSC around RP2350-compatible TinyUSB support.
- Rebuild the SD block backend on top of the new SPI SD implementation.
- Re-test host ejection and write safety carefully.

## 8. Audio

The board still includes `ES8311`, but the current implementation is ESP32 I2S-driver-specific.

Suggested approach:

- Defer audio until display, touch, storage, and reader flow are working.
- Keep the feature optional for the first RP2350 target.

## Lowest-risk port order

1. Add a platform split so the portable app/reader code can build without ESP32-only modules.
2. Bring up display only on RP2350.
3. Enable touch polling.
4. Bring up SD card read-only and load a plain `.rsvp` book.
5. Replace `Preferences` with a cross-platform settings store.
6. Re-enable normal book library flow.
7. Decide whether to ship without Wi-Fi/OTA and without audio for v1 of the RP2350 port.
8. Add USB MSC only after the storage stack is stable.

## Recommended first milestone

The best first milestone is not "full parity." It is:

- boot
- initialize panel
- render UI
- read touch
- open a book from SD
- run RSVP reading

If that milestone works, this board is probably viable as a production replacement even before
USB MSC, audio, or any update workflow lands.
