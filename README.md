# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device for showing text one word at a time with RSVP (Rapid Serial Visual Presentation). The firmware is built around stable anchor-letter rendering, readable typography, tunable pacing, SD card storage, and a browser-first book conversion workflow.

The latest public release is `v0.0.3`. The hosted browser flasher installs the latest published GitHub Release, so features listed under **Coming Soon** are not in the public flasher build until the next release is published.

## Works Now In v0.0.3

### Reader

- One-word RSVP reader with stable anchor alignment.
- Optional page-scroll reading mode using the same pacing and saved-position system.
- Hold to read, double-tap locked autoplay, tap to stop locked autoplay, and sentence-boundary pausing.
- Horizontal scrub preview with hold-to-scroll browsing, then tap to return to RSVP.
- Sentence rewind from the visual left edge.
- Swipe up/down WPM adjustment while paused.
- Chapter and paragraph-aware navigation.
- Saved reading position and reader settings.

### Settings

- Adjustable reading mode, handedness, theme, brightness, and language.
- Adjustable font size, typeface, phantom words, red focus highlight, tracking, anchor position, guide width, and guide gap.
- Adjustable pacing delays for long words, word complexity, and punctuation.
- Menu language selection for English, Spanish, French, German, Romanian, and Polish.
- Wi-Fi setup on the device for OTA updates.

### Library And Conversion

- SD card library under `/books`.
- Supported reader files: `.rsvp`, `.txt`, and `.epub`.
- Browser-side Library Workspace for importing and converting supported books.
- Browser converter support for:
  - `.epub`
  - `.txt`
  - `.md` / `.markdown`
  - `.html` / `.htm` / `.xhtml`
- On-device EPUB conversion fallback when a matching `.rsvp` file does not exist yet.
- Browser cleanup for interrupted conversion sidecar files such as `.rsvp.tmp`, `.rsvp.converting`, and `.rsvp.failed`.

### Firmware Install And Updates

- Browser-based firmware install through ESP Web Tools.
- Optional GitHub Release OTA updates over Wi-Fi.
- USB mass-storage mode for copying books to the SD card in the default USB firmware build.

## Flash v0.0.3 From The Browser

Use the hosted web flasher:

<https://ionutdecebal.github.io/rsvpnano/>

Use Chrome or Edge on desktop, connect the device over USB, and follow the installer prompts. The browser flasher uses ESP Web Tools and Web Serial, so it must be opened over HTTPS or localhost.

The hosted flasher installs the latest published GitHub Release, not unreleased `main` commits.

## Add Books In v0.0.3

Create a `books` folder at the root of the SD card:

```text
/books
  my-book.rsvp
  another-book.epub
```

The best workflow is to use the Library Workspace on the browser flasher page, convert books to `.rsvp`, then sync or copy the converted files into `/books`.

The firmware prioritizes `.rsvp` files. If an EPUB has not been converted yet, the reader can convert it locally the first time it is opened and reuse the generated `.rsvp` file on future launches. The browser converter is still the recommended path for best compatibility.

## Character Support

Current text support is best for ASCII plus a curated set of accented and extended-Latin letters used in many European languages. Common book punctuation such as curly quotes, guillemets, and bracket variants is normalized into readable ASCII wrappers.

The Standard serif reader font renders the wider Latin set directly. Other reader fonts and the tiny UI font fall back to the closest plain ASCII letter where possible. More complex scripts still need additional renderer and font work.

## OTA Updates

The firmware can check GitHub Releases over Wi-Fi and install a newer app build without erasing reader settings or saved reading progress.

To enable OTA on the device:

1. Open `Settings -> Wi-Fi`.
2. Tap `Choose network`.
3. Pick an SSID from the scanned list.
4. Enter the password on the on-device keyboard.
5. Optionally turn on `Auto OTA`.

After that, open `Settings -> Firmware update` to manually check the latest published GitHub Release and install `rsvp-nano-ota.bin` if the release tag is newer than the firmware already on the device.

[`docs/ota.conf.example`](docs/ota.conf.example) is still supported as an optional advanced override or fallback. Copy it to the SD card as `/config/ota.conf` if you want to pre-seed Wi-Fi credentials or change advanced OTA settings.

## Reader Controls

### Hardware Buttons

- `BOOT` short press: cycle brightness.
- `BOOT` hold: cycle theme.
- `PWR` short press from the reader: open the menu.
- `PWR` short press in a submenu: jump back to the main menu.
- `PWR` short press on the main menu: return to the reader.
- `PWR` hold: power off.

### RSVP And Scroll Modes

- Hold on the screen: start reading.
- Release after a hold: pause at the end of the current sentence.
- Double-tap while paused: lock autoplay on.
- Tap while locked autoplay is running: stop at the end of the current sentence.
- Tap the far-left edge: jump to the start of the current sentence, or the previous sentence if you are already at the start.
- Swipe left: scrub backward through the text.
- Swipe right: scrub forward through the text.
- Swipe up while paused: increase WPM.
- Swipe down while paused: decrease WPM.
- Tap the bottom-right footer label: cycle between book progress percent, chapter time left, and book time left.

Horizontal scrubbing in RSVP mode opens a larger preview. Tap to return to the anchored RSVP view, or hold and move vertically to browse through the surrounding text.

## v0.0.3 Menu Map

```text
Main Menu
|- Resume
|- Chapters
|- Library
|- Settings
|  |- Display
|  |  |- Reading mode
|  |  |- L/R Hand
|  |  |- Theme
|  |  |- Brightness
|  |  `- Language
|  |- Typography
|  |  |- Font size
|  |  |- Typeface
|  |  |- Phantom words
|  |  |- Red highlight
|  |  |- Tracking
|  |  |- Anchor
|  |  |- Guide width
|  |  |- Guide gap
|  |  `- Reset
|  |- Word pacing
|  |  |- Long words
|  |  |- Complexity
|  |  |- Punctuation
|  |  |- Time Estimate
|  |  `- Reset pacing
|  |- Wi-Fi
|  `- Firmware update
|- USB transfer
`- Power off
```

## Coming Soon

These changes are already merged after `v0.0.3` and are intended for the next release.

### Device Firmware

- SD card checker from the main menu.
- Faster startup and library loading through metadata skipping and lazy loading.
- Better support for long books and unsupported characters, with clearer error handling.
- More reliable double-tap handling for locked autoplay.
- Pause behavior setting: pause instantly or pause at the end of the sentence.
- Battery display improvements, including percentage or estimated time remaining.
- Pacing-aware time estimates for book and chapter remaining time.
- Backlight handling improvement during standby/deep sleep.
- Configurable OTA GitHub owner in device settings.
- Basic Polish language fixes.

### Library Changes

- Separate folders for books and articles:

```text
/books/books
/books/articles
/config
```

- Separate on-device `Books` and `Articles` pickers.
- Legacy files directly inside `/books` are still read for compatibility.

### iPhone Companion App

- New iPhone companion app in `ios/RSVPNanoCompanion`.
- Companion sync over the reader's temporary `RSVP-Nano-xxxxxx` Wi-Fi network.
- Library page with reader info, books, articles, upload controls, and progress percentages.
- Article page with saved drafts, synced articles, RSS feed management, article editing, and preview before sync.
- Settings page for changing device settings from the phone.
- Help/FAQ page with connection, SD card, and RSS notes.
- Safari and Chrome share flow for saving pages into the app, fetching article text, editing, and syncing later.

### Companion API And Settings

- JSON device settings API:
  - `GET /api/settings`
  - `PATCH /api/settings`
  - `PUT /api/settings`
- RSS feed API:
  - `GET /api/rss-feeds`
  - `PUT /api/rss-feeds`
- Books/articles API improvements for listing, uploading, deleting, and progress reporting.
- Settings are moving toward a sturdier JSON-backed model so the phone app and future web app can edit the same device settings.

### RSS Feeds

- RSS feed list managed from the iPhone app, including offline editing and sync status.
- Device-side RSS checking over saved Wi-Fi.
- Live on-device progress while feeds are checked.
- Articles saved into `/books/articles`.
- Redirect handling for common 301, 302, 303, 307, and 308 responses.
- RSS and Atom parsing for common feed formats.
- Feed item author, `dc:creator`, or website used as the article author.
- Current limits: some feeds block embedded clients, some feeds are too large, and some sites require better article extraction than the firmware can do alone.

### Web And Distribution

- Cleaner firmware installer UI.
- Linux Chromium/Snap USB permission note.
- Public iPhone app distribution still needs to be worked out.
- A device-hosted companion web app is planned for Android and desktop users.
- Bluetooth-first, Wi-Fi-second pairing is planned to make Wi-Fi password entry less painful.
- Larger-book architecture and wider script/font support are planned for a later release.

## Build From Source

Install PlatformIO Core, then run:

```sh
pio run
pio run -t upload
pio device monitor
```

The default environment is `waveshare_esp32s3_usb_msc`, which includes the reader and USB transfer mode. Serial monitor runs at `115200`.

To export the browser-flasher image and OTA binary:

```sh
python3 tools/export_web_firmware.py
```

That command writes:

```text
web/firmware/rsvp-nano.bin
web/firmware/rsvp-nano-ota.bin
```

For OTA releases:

1. Build from a clean commit or tag.
2. Run `python3 tools/export_web_firmware.py`.
3. Create a GitHub Release in `ionutdecebal/rsvpnano`.
4. Upload both `web/firmware/rsvp-nano.bin` and `web/firmware/rsvp-nano-ota.bin`.

The hosted browser flasher and OTA updater both use the latest published GitHub Release assets.

## Hardware

The current firmware targets the [Waveshare ESP32-S3-Touch-LCD-3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=153227):

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

Tests live in `test/test_pacing/` and cover word duration calculation, WPM clamping, and seek/scrub behavior.

## Desktop Converter Fallback

The browser converter is the normal path. The desktop helper is still available for offline or batch conversion:

- Windows: `Convert books.bat`
- macOS: `Convert books.command`
- Linux: `convert_books_linux.sh` or `python3 convert_books.py`

Copy the helper files from `tools/sd_card_converter` to the SD card root and run the launcher for your platform. The desktop converter scans `/books` and creates `.rsvp` files beside supported sources.

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

Issues, experiments, forks, and pull requests are welcome. If you change hardware mappings, build environments, the companion API, or the flashing flow, please update the relevant docs alongside the code.

## License

MIT. See [LICENSE](LICENSE).

The embedded OpenDyslexic and Atkinson Hyperlegible typeface assets are derived from the upstream projects and are included under the SIL Open Font License. See [third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and [third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
