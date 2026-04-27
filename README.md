# RSVP Nano

RSVP Nano is an open-source ESP32 reading device for showing text one word at a time with RSVP (Rapid Serial Visual Presentation). The firmware is built around stable anchor-letter rendering, readable typography, tunable pacing, SD card storage, and local EPUB conversion. Two hardware targets are supported: the original Waveshare ESP32-S3-Touch-LCD-3.49 and the smaller, cheaper Waveshare ESP32-C6-Touch-LCD-1.47.

## Highlights

- One-word RSVP reader with stable anchor alignment.
- Adjustable typography, anchor guides, pacing, and phantom words.
- Chapter and paragraph-aware navigation.
- SD card library under `/books`.
- Local on-device EPUB conversion to cached `.rsvp` files.
- USB mass-storage mode for copying books to the SD card.
- Browser-based firmware installation with no IDE required.

## Getting Started

### Flash From The Browser

The easiest way to install the firmware is the web flasher:

<https://ionutdecebal.github.io/rsvpnano/>

Use Chrome or Edge on desktop, connect the device over USB, and follow the installer prompts.

The browser flasher uses ESP Web Tools and Web Serial, so it must be opened over HTTPS or localhost.

### Add Books

Create a `books` folder at the root of the SD card:

```text
/books
  my-book.epub
  another-book.rsvp
```

The firmware prioritizes `.rsvp` files. If a matching `.rsvp` file does not exist yet, an EPUB appears in the library and is converted locally the first time it is opened. The converted `.rsvp` file is then reused on future launches.

If a conversion is interrupted, you may see sidecar files such as:

```text
.rsvp.tmp
.rsvp.converting
.rsvp.failed
```

## Build From Source

Install PlatformIO Core, then run:

```sh
pio run
pio run -t upload
pio device monitor
```

The default environment is `waveshare_esp32s3_usb_msc`, which includes the reader and USB transfer mode.

To target the Waveshare ESP32-C6-Touch-LCD-1.47 (JD9853 panel + AXS5106L touch, no PSRAM, no native USB-OTG, no on-device EPUB conversion), use:

```sh
pio run -e waveshare_esp32c6
pio run -e waveshare_esp32c6 -t upload
```

The C6 environment ships with these build flags (already wired up in `platformio.ini`):

| Flag | Value | Why |
| --- | --- | --- |
| `RSVP_USB_TRANSFER_ENABLED` | `0` | C6 has no native USB-OTG, so USB mass-storage mode is unavailable. |
| `RSVP_ON_DEVICE_EPUB_CONVERSION` | `0` | No PSRAM; pre-convert EPUBs with the desktop helper instead. |
| `RSVP_MAX_BOOK_WORDS` | `6000` | Caps loaded books to fit in the 512 KB on-chip SRAM. Each `String` word costs ~30–40 B, leaving headroom for SPI/SD buffers and UI state. |

Serial monitor runs at `115200`.

To export the merged binary used by the browser flasher:

```sh
python3 tools/export_web_firmware.py
```

That command writes:

```text
web/firmware/rsvp-nano.bin
```

## Hardware

The current firmware configuration targets the [Waveshare ESP32-S3-Touch-LCD-3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=153227). This is an affiliate link, so if you click it to find the hardware and buy the board, it helps support the project:

- ESP32-S3 with 16 MB flash and OPI PSRAM.
- AXS15231B-based 172 x 640 LCD panel used in landscape as 640 x 172.
- SD card connected through `SD_MMC`.
- Touch, battery, and board power control pins defined in `src/board/BoardConfig.h`.

A second build target — the [Waveshare ESP32-C6-Touch-LCD-1.47](https://www.waveshare.com/esp32-c6-touch-lcd-1.47.htm) — is available as `waveshare_esp32c6`. Notes:

- ESP32-C6 has no PSRAM, so on-device EPUB conversion is disabled in this build. Pre-convert books with the desktop helper instead.
- The C6 has no native USB-OTG, so the USB mass-storage transfer mode is unavailable.
- The display is a 172×320 JD9853 SPI panel driven in 320×172 landscape; the touch controller is an AXS5106L on I2C (address `0x63`, 100 kHz).
- The microSD slot uses SPI on the C6 board and shares the SPI bus with the LCD (`CLK=GPIO1`, `MOSI=GPIO2`, `MISO=GPIO3`, `CS=GPIO4`).
- The board has a single BOOT button (GPIO9). The firmware binds: short press = menu, double press = brightness, long press = theme.
- Touch axis mapping and gesture decoding for the AXS5106L may need calibration for your unit; see [src/input/TouchHandler.cpp](src/input/TouchHandler.cpp).
- Books larger than `RSVP_MAX_BOOK_WORDS` (default 6000) are truncated at load. Raise the cap if you have headroom or split long books into chapters.

### Optional: battery + thermistor on the C6

The C6 board exposes Vbat to GPIO0 through a built-in 1:2 (÷3) divider, so battery monitoring works with no extra parts. For pack temperature, the firmware can read an external NTC thermistor (e.g. the Yellow lead of an LP803448 LiPo) on GPIO6:

```text
3V3 ──[ 10 kΩ ]──┬── GPIO6 (ADC1_CH6)
                 └── NTC (Yellow) ── BAT-/GND
```

Defaults assume an NTC with R0 = 10 kΩ at 25 °C and β = 3950 (typical for LP-series LiPos). Override `THERMISTOR_BETA`, `THERMISTOR_NOMINAL_OHMS`, or `THERMISTOR_NTC_TO_GND` in [src/board/BoardConfig.h](src/board/BoardConfig.h) if your pack differs. With no thermistor connected the firmware reports voltage only.

If you are adapting the project to different hardware, start with [src/board/BoardConfig.h](src/board/BoardConfig.h), then review the display, touch, power, and SD wiring code.

## Running Tests

The pacing algorithm has a host-side unit test suite that runs without hardware using PlatformIO's native environment.

```sh
pio test -e native_test
```

Tests live in `test/test_pacing/` and cover word duration calculation (length tiers, syllable complexity, punctuation pauses, abbreviation detection, pacing scale), WPM clamping, and seek/scrub behaviour. A minimal `Arduino.h` shim in `test/support/` lets `ReadingLoop.cpp` compile on the host without the ESP32 SDK.

## Desktop Book Conversion

If you prefer to pre-convert books on a computer, copy the helper files from `tools/sd_card_converter` to the SD card root and run the launcher for your platform:

- Windows: `Convert books.bat`
- macOS: `Convert books.command`
- Linux: `convert_books_linux.sh` or `python3 convert_books.py`

The desktop converter scans `/books` and creates `.rsvp` files beside supported sources.
The Linux path has been used during development. The macOS and Windows launchers are included, but they have not been tested yet.

Supported input formats:

- `.epub`
- `.txt`
- `.md` / `.markdown`
- `.html` / `.htm` / `.xhtml`

## RSVP File Format

`.rsvp` files are plain text. The reader understands a small set of directives:

```text
@rsvp 1
@title The Book Title
@author Author Name
@source /books/source.epub
@chapter Chapter 1
@para
```

Normal text lines after the directives are split into words by the firmware.

## Contributing

Issues, experiments, forks, and pull requests are welcome. If you change hardware mappings, build environments, or the flashing flow, please update the relevant docs alongside the code.

## License

MIT. See [LICENSE](LICENSE).
