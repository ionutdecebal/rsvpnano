# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device that shows text one word at a time using RSVP, Rapid Serial Visual Presentation. It is designed for small screens, SD card libraries, fast reading, and a simple browser-first workflow for converting and uploading books.

This README is written for the current release, `v0.0.8`.

## What You Need

- An RSVP Nano device.
- A USB-C data cable.
- A microSD card.
- Chrome or Edge on a desktop computer for browser flashing and the web converter.
- Optional: native iOS or Android companion apps built locally while public distribution is pending.

## Supported Hardware

The browser flasher supports these device targets:

- Waveshare ESP32-S3 Touch LCD 3.49 rev1.
- Waveshare ESP32-S3 Touch LCD 3.49 rev2.
- Waveshare ESP32-S3 Touch AMOLED 1.8 V1.
- Waveshare ESP32-S3 Touch AMOLED 1.8 V2 Test.
- Waveshare ESP32-S3 Touch AMOLED 2.16.
- Waveshare ESP32-S3 Touch AMOLED 2.41.

Some hardware links below are affiliate links. Buying through them may support RSVP Nano at no
extra cost to you:

- [ESP32-S3 Touch LCD 3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=ionutdecebal)
- [ESP32-S3 Touch AMOLED 1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm?&aff_id=ionutdecebal)
- [ESP32-S3 Touch AMOLED 2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=ionutdecebal)
- [ESP32-S3 Touch AMOLED 2.41](https://www.waveshare.com/esp32-s3-touch-amoled-2.41.htm?&aff_id=ionutdecebal)

## Quick Start

1. Flash the firmware from the browser.
2. Format the SD card and create the library folders.
3. Convert books or articles to `.rsvp`.
4. Copy or upload files to the device.
5. Pick a book or article from the device menu and start reading.

## Flash The Firmware

Use the hosted flasher:

<https://ionutdecebal.github.io/rsvpnano/>

Open it in Chrome or Edge on desktop, connect the device over USB, and follow the installer prompts. The flasher uses ESP Web Tools and Web Serial, so it must run from HTTPS or localhost.

Choose the correct device from the flasher dropdown before installing.

Most ESP32-S3 Touch LCD 3.49 devices should use the default rev1 firmware option. If a newer
Waveshare batch boots but brightness or backlight control does not respond, try the rev2 firmware
option instead; it uses the alternate GPIO42 backlight profile.

For Waveshare Touch AMOLED 1.8 boards, choose the V1 option for SH8601 display / FT3168 touch
hardware. Choose the V2 Test option only for newer CO5300 display / CST816 touch hardware.

The hosted flasher installs the latest published GitHub Release. For `v0.0.8`, that means the
release build includes the firmware, SD card, RSS, companion sync, USB transfer, quick settings,
browser flasher, menu, input, battery, display, multi-board, compact timer, and one-handed reader
control work described below.

Make sure your USB cable is a data cable.

## Prepare The SD Card

Use a microSD card formatted as FAT32.

- 8 GB to 32 GB cards are the safest choice.
- 64 GB cards can work, but they usually need to be reformatted as FAT32 with a single partition.
- exFAT is not the recommended format for this firmware.

Create these folders on the card:

```text
/books/books
/books/articles
/config
```

Books go in `/books/books`. Articles go in `/books/articles`. Older libraries with files directly inside `/books` are still read for compatibility, but the split folders are the recommended layout for `v0.0.8`.

If the device cannot see the SD card, the most common causes are:

- The card is exFAT instead of FAT32.
- The card has multiple partitions.
- The folders are missing or named differently.
- The card was removed without ejecting it from the computer.
- The card is slow, worn out, or unreliable.

The device includes an `SD card check` tool under `Settings` to help diagnose card size, mount status, write access, and folder layout.

## Convert Books And Articles

The recommended conversion workflow is still the browser converter on the hosted flasher page:

<https://ionutdecebal.github.io/rsvpnano/>

Use the converter to turn supported files into `.rsvp`, then upload or copy the `.rsvp` files to the device.

Supported converter inputs include:

- `.epub`
- `.txt`
- `.md` / `.markdown`
- `.html` / `.htm` / `.xhtml`

The firmware can still open `.txt` files and has an on-device EPUB fallback, but the browser converter is the best path for large books, cleaner formatting, and fewer surprises.

## Add Files To The Device

### Option 1: Copy To The SD Card

Power the device off, remove the SD card, copy files from your computer, then reinsert the card.

Use this layout:

```text
/books/books/my-book.rsvp
/books/articles/my-article.rsvp
```

On first open, the firmware may create `.ridx` and `.rdat` sidecar files next to a book. These are the SD-backed word index and normalized word data used for long books. Leave them on the card; they are rebuilt automatically if the source book changes.

Large books now load through the same indexed reading path as smaller books, with progress messages while indexes and time estimates are prepared. If a book cannot be prepared, the device should return to the menu with a readable reason instead of silently failing.

### Option 2: USB Transfer Mode

From the device:

1. Swipe up from the bottom edge to open quick settings.
2. Choose `Sync`.
3. Choose `USB Sync`.
4. Copy `.rsvp` files from your computer.
5. Eject the device from the computer.
6. Wait for the device to remount the SD card and refresh the library.

Always eject before leaving USB transfer mode where possible. After the host ejects it, the device
remounts the SD card and refreshes the library.

### Option 3: Web Companion

The device can host its own browser companion page.

1. Swipe up from the bottom edge to open quick settings.
2. Choose `Sync`.
3. Choose `Wi-Fi Sync`.
4. The device shows the Wi-Fi network name and the browser URL.
5. Connect your phone, tablet, or computer to the `RSVP-Nano-xxxxxx` Wi-Fi network.
6. Open the URL shown on the device, usually `http://192.168.4.1`.

The web companion has pages for:

- `Books`: upload book `.rsvp` files and view the book library.
- `Articles`: write, paste, edit, preview, and upload articles.
- `Settings`: edit device settings and save home Wi-Fi credentials.
- `RSS`: manage RSS feed URLs.
- `Help`: quick notes for connection, conversion, SD cards, and RSS.

The web companion remains the easiest option from desktop browsers and for anyone who does not have
a native companion app installed.

### Option 4: Native Companion Apps

The iOS and Android companion apps support companion sync, article drafts, share/import flows, RSS
feed management, device settings, and library progress.

Public app distribution is not set up yet. The iOS app can be installed from a Mac with Xcode, and
the Android app can be built and installed with Android Studio or the Android SDK.

See:

[`RSVPNanoCompanion/ios/RSVPNanoCompanion/README.md`](RSVPNanoCompanion/ios/RSVPNanoCompanion/README.md)

[`RSVPNanoCompanion/androidApp/README.md`](RSVPNanoCompanion/androidApp/README.md)

## Home Wi-Fi, RSS, And OTA

The device can save home Wi-Fi credentials for features that need internet access, such as RSS feed checks and OTA firmware updates.

You can set Wi-Fi credentials from:

- The web companion `Settings` page.
- The native companion app settings page.
- The on-device Wi-Fi settings page.
- Advanced users can still use `/config/ota.conf`.

RSS feeds are managed from the web companion or the native app, then checked from the device with
`Articles -> Update RSS`. New articles are saved into `/books/articles`.

RSS support in `v0.0.8` includes:

- RSS and Atom feed parsing.
- Redirect handling for common `301`, `302`, `303`, `307`, and `308` responses.
- Live on-device progress while feeds are checked.
- Duplicate skipping.
- Feed item author, creator, or website name used as the article source.
- Larger feed downloads than earlier test builds.
- Better handling for feeds that send usable complete items before timing out.
- Longer full-text article bodies before on-device truncation.

Some feeds still block embedded clients, require JavaScript, return very large pages, or publish summaries instead of full articles. Those are feed or website limitations rather than SD card problems.

OTA updates use GitHub Releases. Open `Settings -> Firmware update` on the device after Wi-Fi is configured.

## Device Controls

The current UI is built around edge gestures, a small top-level menu, and quick settings. On paused
reader screens, subtle handles at the top and bottom edges hint that those menus are available.

### Hardware Buttons

- Swipe down from the top edge: open the main menu.
- Swipe up from the bottom edge: open quick settings.
- Swipe right in menus: go back one level, or close the top-level menu.
- On 3.49 rev1/rev2 and 2.41, `PWR` tap opens or closes the main menu.
- On 3.49 rev1/rev2 and 2.41, `PWR` hold opens the power-off flow.
- On 3.49 rev1/rev2 and 2.41, `BOOT` tap toggles play/pause in the reader and acts as Back in menus.
- On 3.49 rev1/rev2 and 2.41, `BOOT` hold enters standby/screensaver.
- On 2.16, `PWR` tap opens or closes the main menu and `PWR` hold opens the power-off flow.
- On 2.16, `BOOT` tap cycles brightness and `BOOT` hold cycles the display theme.
- On 2.16, `KEY` tap toggles play/pause and `KEY` hold enters standby/screensaver.
- On 1.8, firmware ignores the unreliable PWR input. Use swipe down for the main menu, swipe up for quick settings, `BOOT` tap for play/pause or Back, and `BOOT` hold for standby/screensaver.
- USB Transfer exits automatically when the computer ejects the device. On boards with firmware PWR input, holding `PWR` can also leave USB Transfer after copying is finished.

### Reader Controls

- Tap the rewind edge: left by default, or top-right when Reader controls is set to Rewind top-right. Rewinds to the start of the current sentence, or the previous sentence if you are already at the start.
- Swipe left or right while paused: scrub through nearby text.
- Tap after scrubbing: return to RSVP view.
- Hold and move vertically in the scrub preview: browse through surrounding text.
- Swipe up while paused: increase WPM.
- Swipe down while paused: decrease WPM.
- Tap the bottom-right footer label: switch between progress, chapter time remaining, book time remaining, and battery display modes.
- Tap the top-right battery label: switch between percentage, time remaining, and voltage.

Pause behavior is configurable. In `Settings -> Word pacing`, choose whether reader shortcuts pause instantly or at the end of the sentence.

### Quick Settings

Open quick settings by swiping up from the bottom edge.

```text
Brightness
Theme
Focus Timer
Sync
```

`Brightness` cycles through the brightness presets. `Theme` cycles Dark, Light, Night, and Yellow.
`Focus Timer` opens the orientation-based timer. `Sync` opens a second menu:

```text
Wi-Fi Sync
USB Sync
```

`Wi-Fi Sync` starts the device-hosted companion page for browser or native app sync. `USB Sync`
starts USB mass-storage transfer so files can be copied without removing the SD card.

### Main Menu

Open the main menu with a top-edge swipe. On boards with firmware PWR input, `PWR` tap also opens
or closes this menu.

```text
Resume
Chapters
Books
Articles
Settings
Power off
```

Swipe up or down to move through the menu. Tap to select. Swipe right to go back one level or close
the top-level menu. Submenus keep an on-screen `Back` item at the top. On boards where `BOOT` acts
as Back, pressing it returns one level or closes the menu.

### Books And Articles

`Books` shows files from `/books/books`.

`Articles` opens a small submenu for browsing saved articles or updating RSS feeds.

Both pages show readable titles, progress, and saved position where available. Select an item to load it into the reader.

### Chapters

The `Chapters` page lists chapter markers from the current book when available. Select a chapter to
jump to it. Use the on-screen `Back` item, `BOOT` Back where available, or `PWR` where available to
return to the main menu.

### Settings

Settings are grouped by how people actually use the device.

`Display` includes:

- Display theme.
- Brightness.
- Left/right handed layout.
- Reader controls layout, including an option to put rewind in the top-right corner.
- Language.
- Screen saver: Life, Maze, Voronoi, or Screen off.
- Standby timer.
- Footer and battery label behavior.
- Optional battery, chapter, and book percentage labels while actively reading.
- Menu repeat speed.

`Typography` includes:

- Font size.
- Typeface.
- Phantom words.
- Red focus highlight.
- Tracking.
- Anchor position.
- Guide width.
- Guide gap.
- Typography preview and reset.

`Word pacing` includes:

- RSVP or scroll reading behavior.
- Instant pause or sentence-end pause.
- Long-word delay.
- Complexity delay.
- Punctuation delay.
- Pacing reset.

`Wi-Fi` includes:

- Saved network selection.
- Choose or forget network.
- Auto OTA.
- OTA owner/source.

`Battery` includes:

- CPU speed for RSVP, scroll, paused, menu, and standby states.
- Auto-dim delay.
- Auto-dim brightness level.

`Firmware update` checks GitHub Releases and installs newer firmware when available. `SD card
check` also lives under Settings.

### Companion Sync

Use this page to connect the native companion app or the web companion.

1. Swipe up from the bottom edge.
2. Choose `Sync`.
3. Choose `Wi-Fi Sync`.
4. Connect to the Wi-Fi network shown on the device.
5. Open the URL shown on the device.
6. Use the web companion or native companion app.
7. Exit from the device when finished.

When Companion Sync exits, the device reloads settings and refreshes the library.

### USB Transfer

Use this page to copy files over USB without removing the SD card.

1. Swipe up from the bottom edge.
2. Choose `Sync`.
3. Choose `USB Sync`.
4. Copy files from your computer.
5. Eject the device from the computer.

Always eject before leaving USB transfer mode where possible. The device remounts the SD card and
refreshes the library after the host ejects it. On boards with firmware PWR input, holding `PWR` can
also leave USB Transfer.

### RSS Feeds

Use the web companion or native app to manage feed URLs. Then open `Articles -> Update RSS` on the
device.

The device shows live progress as it checks feeds. Saved articles appear in `Articles`.

If a feed cannot be downloaded, the reader shows a plain-English reason such as `Feed not found`, `Site blocked reader`, or `Site took too long`.

RSS checks can continue in the background, while installable firmware updates still ask for confirmation before the device changes itself.

### Focus Timer

The Focus Timer uses the device orientation to guide work and break blocks.

1. Swipe up from the bottom edge.
2. Choose `Focus Timer`.
3. Choose a timer category.
4. Place or flip the device as prompted.
5. Follow the on-screen timer.
6. Use Back or `PWR` where available to exit the timer page.

Touch-and-hold during an active timer cancels the current timer block.

### SD Card Check

Run `Settings -> SD card check` if books or articles do not appear. It checks whether the card
mounts, whether it can write, and whether the expected library folders exist.

If the old folder layout needs repair, the device now asks before changing the card.

## Character Support

`v0.0.8` includes the long-book and unsupported-character improvements from earlier releases. Common punctuation is normalized, ellipses and hyphenated sentence breaks are handled more carefully, and many accented Latin characters render directly or fall back to readable plain Latin equivalents.

The current renderer is best for English and European Latin-script languages. Complex scripts still need additional font and shaping work.

## Companion App Status

The native companion apps are working locally, but public distribution is not set up yet.

Current app features include:

- Library view for books and articles.
- Article drafts, editing, preview, and sync.
- Share/import flows.
- Fetch article title and text where available.
- Device settings editor.
- RSS feed management.
- Help and FAQ pages.

Temporary install instructions are in:

[`RSVPNanoCompanion/ios/RSVPNanoCompanion/README.md`](RSVPNanoCompanion/ios/RSVPNanoCompanion/README.md)

[`RSVPNanoCompanion/androidApp/README.md`](RSVPNanoCompanion/androidApp/README.md)

## Build From Source

Firmware builds with PlatformIO:

```bash
pio run
```

All firmware targets enable USB transfer mode. The default build is the Touch LCD 3.49
rev1 target; use explicit targets for other boards:

```bash
pio run -e waveshare_esp32s3_rev2
pio run -e waveshare_esp32s3_touch_amoled_18
pio run -e waveshare_esp32s3_touch_amoled_18_v2
pio run -e waveshare_esp32s3_touch_amoled_241
pio run -e waveshare_esp32s3_touch_amoled_216
```

Upload to a connected device:

```bash
pio run -t upload
```

Monitor serial output:

```bash
pio device monitor
```

The iOS app lives in:

```text
RSVPNanoCompanion/ios/RSVPNanoCompanion
```

Open the Xcode project from that folder when installing the app locally.

To export browser-flasher and OTA firmware assets for a release:

```bash
python3 tools/export_web_firmware.py --version v0.0.8
```

That writes:

```text
web/firmware/rsvp-nano.bin
web/firmware/rsvp-nano-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin
web/firmware/rsvp-nano-rev2.bin
web/firmware/rsvp-nano-rev2-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-1.8.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-1.8-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-1.8-v2-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-2.16.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-2.16-ota.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-2.41.bin
web/firmware/rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin
web/firmware/manifest.json
web/firmware/manifest-rev2.json
web/firmware/manifest-esp32-s3-touch-amoled-1.8.json
web/firmware/manifest-esp32-s3-touch-amoled-1.8-v2.json
web/firmware/manifest-esp32-s3-touch-amoled-2.16.json
web/firmware/manifest-esp32-s3-touch-amoled-2.41.json
```

## Project Status

`v0.0.8` focuses on reader touch ergonomics and more resilient RSS downloads:

- Restricts top and bottom edge menu swipe areas toward the middle of the display so WPM changes are
  less likely to open menus by mistake.
- Adds right-swipe Back navigation throughout menus, including text entry.
- Adds a Reader controls setting that can put rewind in the top-right corner for one-handed use,
  while moving the battery label to the top-left.
- Exposes the Reader controls setting on device, in the device-hosted web companion, and in the
  shared native companion app.
- Raises RSS feed and article size limits for longer full-text feeds.
- Accepts usable partial RSS or Atom downloads when complete items arrive before a slow feed times
  out.

`v0.0.7` focused on multi-board release polish and the fixes found during hardware testing:

- Adds the Waveshare Touch AMOLED 1.8 V2 Test firmware target for newer CO5300 display / CST816
  touch hardware.
- Keeps the original Touch AMOLED 1.8 target labeled as V1, for SH8601 display / FT3168 touch
  hardware.
- Adds the 1.8 V2 Test target to the hosted browser flasher, release asset export, release
  download helper, and GitHub release workflows.
- Enables USB SD-card transfer mode across all firmware targets and fixes the transfer path to use
  the mounted FatFS SD drive.
- Fixes Touch AMOLED 2.16 touch input by polling the CST92xx controller instead of depending on the
  unreliable interrupt line.
- Improves the focus timer layout on compact AMOLED screens, including centered countdown text,
  non-overlapping Restart text on 1.8, and clearer break placement instructions.
- Adds benchmark firmware targets for display, touch, audio, SD, and EPUB conversion checks.
- Shares board display, storage, IMU, and GPIO-expander helper code so board profiles stay more
  consistent.

`v0.0.6` introduced the new cross-device input model, restructured menu, bottom-edge quick
settings, Wi-Fi/USB Sync picker, browser flasher device dropdown, AMOLED 1.8, 2.16, and 2.41
web-flash assets, USB transfer across firmware targets, battery and auto-dim settings, and the
paused-reader edge handles that hint at the swipe menus.

The next areas of work are:

- Public app distribution.
- More Android/iOS device testing and polish.
- More capable article extraction for sites that do not expose full RSS content.
- A fuller browser-hosted companion experience for desktop and Android users.
- More font and script support.

## License

MIT. See [LICENSE](LICENSE).

The embedded OpenDyslexic and Atkinson Hyperlegible typeface assets are derived from the upstream projects and are included under the SIL Open Font License. See [third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and [third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
