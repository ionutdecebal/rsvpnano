# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device for showing text one word at a time with RSVP (Rapid Serial Visual Presentation). The firmware is built around stable anchor-letter rendering, readable typography, tunable pacing, SD card storage, and local EPUB conversion.

## Highlights

- One-word RSVP reader with stable anchor alignment.
- Adjustable typeface, font size, typography, anchor guides, pacing, and phantom words.
- Chapter and paragraph-aware navigation.
- SD card library under `/books`.
- Local on-device EPUB conversion to cached `.rsvp` files.
- USB mass-storage mode for copying books to the SD card.
- Browser-based firmware installation plus in-browser library conversion, sidecar cleanup, and SD-card sync.

## Getting Started

### Flash From The Browser

The easiest way to install the firmware is the web flasher:

<https://ionutdecebal.github.io/rsvpnano/>

Use Chrome or Edge on desktop, connect the device over USB, and follow the installer prompts.

The browser flasher uses ESP Web Tools and Web Serial, so it must be opened over HTTPS or localhost.
It also includes a browser-side Library Workspace for importing supported books, converting them
into `.rsvp`, downloading a `.zip` of the results, cleaning interrupted sidecar files, and syncing
the converted outputs back into the SD card's `/books` folder.

The browser workflow currently accepts:

- `.epub`
- `.txt`
- `.md` / `.markdown`
- `.html` / `.htm` / `.xhtml`

The default export mode is `Device-safe ASCII`, which fits the current firmware best. A
`Preserve Unicode` mode is also available for experimentation and future renderer work.

### Add Books

The easiest workflow is to use the Library Workspace on the browser flasher page, then sync the
converted files directly into the SD card's `/books` folder.

If you want to manage files manually, create a `books` folder at the root of the SD card:

```text
/books
  my-book.epub
  another-book.rsvp
```

The device library scans `/books` for `.rsvp`, `.txt`, and `.epub` files.

The firmware prioritizes `.rsvp` files. If a matching `.rsvp` file does not exist yet, an EPUB
appears in the library and is converted locally the first time it is opened. The converted `.rsvp`
file is then reused on future launches.

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

If you are adapting the project to different hardware, start with `src/board/BoardConfig.h`, then review the display, touch, power, and SD wiring code.

## Running Tests

The pacing algorithm has a host-side unit test suite that runs without hardware using PlatformIO's native environment.

```sh
pio test -e native_test
```

Tests live in `test/test_pacing/` and cover word duration calculation (length tiers, syllable complexity, punctuation pauses, abbreviation detection, pacing scale), WPM clamping, and seek/scrub behaviour. A minimal `Arduino.h` shim in `test/support/` lets `ReadingLoop.cpp` compile on the host without the ESP32 SDK.

## Optional Desktop Book Conversion

You do not need the desktop converter for the normal workflow anymore. The browser flasher page can
already convert supported books locally and sync them into the SD card.

The desktop helper is still available if you want an offline, script-driven, or batch-conversion
path on a computer. To use it, copy the helper files from `tools/sd_card_converter` to the SD card
root and run the launcher for your platform:

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

The embedded OpenDyslexic and Atkinson Hyperlegible typeface assets are derived from the upstream
projects and are included under the SIL Open Font License. See
[third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and
[third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
