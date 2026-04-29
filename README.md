# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device for showing text one word at a time with RSVP (Rapid Serial Visual Presentation). The firmware is built around stable anchor-letter rendering, readable typography, tunable pacing, SD card storage, and a web-first book conversion workflow.

## Highlights

- One-word RSVP reader with stable anchor alignment.
- Optional page-scroll reading mode that keeps the same pacing/book-position mechanics without the RSVP anchor.
- Hold to read, double-tap to lock autoplay, tap to stop locked autoplay, and pause on sentence boundaries.
- Horizontal scrub preview with hold-to-scroll text browsing, then tap to return to RSVP.
- Adjustable typeface, font size, typography, anchor guides, pacing, and phantom words.
- Menu language selection for English, Spanish, French, German, Romanian, and Polish.
- Chapter and paragraph-aware navigation.
- SD card library under `/books`.
- Web-first book conversion and SD-card library sync from the browser flasher.
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

On the device, you can switch the menu language in `Settings -> Display -> Language`.
You can also switch between anchored RSVP and the page scroller in `Settings -> Display ->
Reading mode`.
While paused in RSVP mode, swipe left or right to open the larger scrub preview, hold and move
your finger vertically to browse smoothly through the text, and tap to return to the anchored
word view.

The browser workflow currently accepts:

- `.epub`
- `.txt`
- `.md` / `.markdown`
- `.html` / `.htm` / `.xhtml`

The browser page automatically writes `.rsvp` output that preserves common accented, Baltic,
Sami, and other extended-Latin letters while staying compatible with the current firmware.
It is currently the best-supported conversion path and the recommended way to prepare books.

### Add Books

The easiest workflow is to use the Library Workspace on the browser flasher page, then sync the
converted files directly into the SD card's `/books` folder.

If you want to manage files manually, create a `books` folder at the root of the SD card:

```text
/books
  my-book.epub
  another-book.rsvp
```

The device library scans `/books` for `.rsvp`, `.txt`, and `.epub` files, but the recommended
workflow is to put browser-converted `.rsvp` files there whenever possible.

Current text support is best for ASCII plus a curated set of accented and extended-Latin letters
used in many European languages. That includes the usual Germanic and Nordic letters plus common
extras such as `OE`/`oe` ligatures, Polish `L`-slash style letters, Romanian comma-accent letters,
several Central European and Turkish forms, the Czech and Hungarian letters used outside Latin-1,
and the Baltic/Sami letters used in Latvian, Lithuanian, and Sami text. Common book punctuation
such as curly quotes, guillemets, and bracket variants is normalized into readable ASCII wrappers.
The Standard serif reader font renders that wider Latin set directly. In the other reader fonts
and in the tiny UI font, unsupported letters currently fall back to the closest plain ASCII letter
in the selected font instead of switching fonts.
More complex scripts still need additional renderer and font work.

The firmware prioritizes `.rsvp` files. If a matching `.rsvp` file does not exist yet, an EPUB
can still be converted locally the first time it is opened, and the converted `.rsvp` file is then
reused on future launches. That on-device path is best treated as a fallback; the browser converter
currently has the best compatibility and library-management flow.

If a conversion is interrupted, you may see sidecar files such as:

```text
.rsvp.tmp
.rsvp.converting
.rsvp.failed
```

## User Guide

Most reader settings and your current reading position are saved automatically.

### Hardware Buttons

- `BOOT` short press: cycle brightness.
- `BOOT` hold: cycle theme (`Dark -> Light -> Night -> Dark`).
- `PWR` short press: open the menu from the reader.
- `PWR` short press while in a submenu: jump back to the main menu.
- `PWR` short press while on the main menu: return to the reader.
- `PWR` hold: power the device off.

### Reader Shortcuts

#### RSVP Mode

- Hold on the screen: start reading.
- Release after a hold: pause at the end of the current sentence.
- Double-tap while paused: lock autoplay on.
- Tap while locked autoplay is running: stop at the end of the current sentence.
- Swipe left: scrub backward through the text.
- Swipe right: scrub forward through the text.
- Swipe up while paused: increase WPM.
- Swipe down while paused: decrease WPM.

Horizontal scrubbing in RSVP mode opens a larger preview. In that preview:

- Tap: return to the anchored RSVP view.
- Hold, then move your finger in the top half of the screen: scroll upward.
- Hold, then move your finger in the bottom half of the screen: scroll downward.
- Moving farther from the center increases browse speed.

#### Page Scroll Mode

- Hold on the screen: start reading.
- Release after a hold: pause at the end of the current sentence.
- Double-tap while paused: lock autoplay on.
- Tap while locked autoplay is running: stop at the end of the current sentence.
- Swipe left: scrub backward through the text.
- Swipe right: scrub forward through the text.
- Swipe up while paused: increase WPM.
- Swipe down while paused: decrease WPM.

Page scroll mode keeps the same pacing and saved-position behavior as RSVP mode, but it shows a
larger scrolling text view instead of the anchored word display.

### Menu Navigation

- Open the menu with the `PWR` button.
- Swipe up or down to move the selection.
- Tap to activate the highlighted item.
- In the typography tuning screen, swipe left or right to change the preview sample word.
- In the typography tuning screen, tap the selected control to cycle or toggle that setting.

### Menu Map

```text
Main Menu
|- Resume
|- Chapters
|  |- Back
|  |- Start of Book or chapter list
|  `- Restart Book
|     |- No, keep place
|     `- Yes, restart
|- Library
|  |- Back
|  `- Book list
|- Settings
|  |- Back
|  |- Display
|  |  |- Back
|  |  |- Reading mode
|  |  |- Theme
|  |  |- Brightness
|  |  `- Language
|  |- Typography
|  |  |- Back
|  |  |- Font size
|  |  |- Typeface
|  |  |- Phantom words
|  |  |- Red highlight
|  |  |- Tracking
|  |  |- Anchor
|  |  |- Guide width
|  |  |- Guide gap
|  |  `- Reset
|  `- Word pacing
|     |- Back
|     |- Long words
|     |- Complexity
|     |- Punctuation
|     `- Reset pacing
|- USB transfer (default USB build)
`- Power off
```

### Settings Reference

#### Display

- `Reading mode`: switch between anchored RSVP and page scroll.
- `Theme`: cycle `Dark`, `Light`, and `Night`.
- `Brightness`: cycle the backlight level.
- `Language`: cycle `English`, `Espanol`, `Francais`, `Deutsch`, `Romana`, and `Polski`.

#### Typography

- `Font size`: cycle `Large`, `Medium`, and `Small`.
- `Typeface`: cycle `Standard`, `Atkinson`, and `OpenDyslexic`.
- `Phantom words`: show or hide the surrounding helper words in RSVP mode.
- `Red highlight`: turn the focus-letter highlight on or off in RSVP mode.
- `Tracking`: adjust letter spacing.
- `Anchor`: move the RSVP focus position horizontally.
- `Guide width`: change the width of the anchor guides.
- `Guide gap`: change the gap between the anchor guides.
- `Reset`: restore the typography settings to their defaults.

#### Word Pacing

- `Long words`: add extra delay to longer words.
- `Complexity`: add extra delay to more complex words.
- `Punctuation`: add extra delay around sentence rhythm and punctuation.
- `Reset pacing`: restore pacing delays to their defaults.

### USB Transfer

On the default USB-enabled firmware build, open `USB transfer` from the main menu to expose the SD
card over USB. When you are done copying books:

1. Eject the device from your computer.
2. Hold `BOOT` to leave USB transfer mode.
3. Wait for the device to remount the SD card and return to the reader.

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

## Desktop Converter Fallback

You do not need the desktop converter for the normal workflow. The browser flasher page is the
recommended conversion path and can already convert supported books locally and sync them into the
SD card.

The desktop helper is still available if you want an offline, script-driven, or batch-conversion
fallback on a computer. To use it, copy the helper files from `tools/sd_card_converter` to the SD
card root and run the launcher for your platform:

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
