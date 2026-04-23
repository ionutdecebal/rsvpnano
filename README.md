# RSVP Nano v2

RSVP Nano v2 is a small ESP32-S3 speed-reading device for reading books one word at a time using RSVP (rapid serial visual presentation). The firmware focuses on stable word anchoring, readable typography, adjustable pacing, SD card storage, and local book conversion.

This repository is currently a hardware/firmware beta. It is usable, but the board wiring and display driver are still specific to the current prototype.

The project is open source under the MIT License.

## Current Features

- RSVP reader with focused anchor-letter rendering.
- Adjustable typography, anchor position, guide marks, and word pacing.
- Chapter and paragraph-aware navigation.
- SD card library under `/books`.
- Local on-device EPUB conversion to `.rsvp` cache files.
- Desktop SD-card converter for EPUB, text, Markdown, and HTML sources.
- Optional USB mass-storage transfer build for copying books to the SD card.
- Battery status, light sleep, and power-hold handling for the current board.

## Hardware Target

The current firmware target is an ESP32-S3 board profile with:

- ESP32-S3, 16 MB flash, OPI PSRAM.
- AXS15231B-based 172 x 640 LCD panel used in landscape as 640 x 172.
- SD card wired through `SD_MMC`.
- Touch and board power control pins defined in `src/board/BoardConfig.h`.

Pin assignments are in `src/board/BoardConfig.h`. If you are building different hardware, start there and expect to update the display, touch, power, and SD wiring.

## Build And Flash

### Browser Flasher

For most people, the easiest route is the browser flasher in `web/index.html`. When published with GitHub Pages, it lets someone flash the firmware from Chrome or Edge without installing VS Code, PlatformIO, or Python.

The flasher uses ESP Web Tools and Web Serial. It requires HTTPS or localhost.

To build the merged firmware binaries used by the web flasher:

```sh
python3 tools/export_web_firmware.py
```

That script builds both firmware environments and writes:

```text
web/firmware/rsvp-nano-v2.bin
web/firmware/rsvp-nano-v2-usb-msc.bin
```

The `.bin` files are ignored locally; the GitHub Pages workflow generates and publishes them automatically.

### Developer Build

Install PlatformIO Core, then from the repository root:

```sh
pio run
pio run -t upload
pio device monitor
```

The default environment is:

```text
waveshare_esp32s3
```

For the USB mass-storage transfer build:

```sh
pio run -e waveshare_esp32s3_usb_msc
pio run -e waveshare_esp32s3_usb_msc -t upload
```

Serial monitor runs at `115200`.

## SD Card Layout

Create a `books` folder at the root of the SD card:

```text
/books
  my-book.epub
  another-book.rsvp
```

The firmware prioritizes `.rsvp` files. EPUB files appear in the library only while they do not have a matching `.rsvp` cache beside them. When you open an EPUB, the device shows `Preparing book`, converts it locally, writes a `.rsvp` file, and then loads that cached version on future opens.

Conversion sidecar files may appear if a conversion is interrupted:

```text
.rsvp.tmp
.rsvp.converting
.rsvp.failed
```

If you are actively testing conversion, the serial monitor is the best debugging view.

## Desktop Converter

For faster or pre-flight conversion, copy the helper files from `tools/sd_card_converter` to the SD card root and run the launcher for your computer:

- Windows: `Convert books.bat`
- macOS: `Convert books.command`
- Linux: `convert_books_linux.sh` or `python3 convert_books.py`

The desktop converter scans `/books` and creates `.rsvp` files beside supported sources.

Supported desktop input formats:

- `.epub`
- `.txt`
- `.md` / `.markdown`
- `.html` / `.htm` / `.xhtml`

## RSVP File Format

`.rsvp` files are plain text. The reader understands a few simple directives:

```text
@rsvp 1
@title The Book Title
@author Author Name
@source /books/source.epub
@chapter Chapter 1
@para
```

Normal text lines after the directives are split into words by the firmware.

## Project Status

This is a fast-moving prototype repo. Before a broader public release, choose and add a license, confirm the exact hardware revision, and run the release checklist in `docs/RELEASE_CHECKLIST.md`.
