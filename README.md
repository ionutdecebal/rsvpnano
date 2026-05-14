# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device that shows text one word at a time using RSVP, Rapid Serial Visual Presentation. It is designed for small screens, SD card libraries, fast reading, and a simple browser-first workflow for converting and uploading books.

This README is written for the current release, `v0.0.4`.

## What You Need

- An RSVP Nano device.
- A USB-C data cable.
- A microSD card.
- Chrome or Edge on a desktop computer for browser flashing and the web converter.
- Optional: an iPhone companion app installed from Xcode while TestFlight approval is pending.

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

The hosted flasher installs the latest published GitHub Release. For `v0.0.4`, that means the release build includes the firmware, SD card, RSS, companion sync, web companion, and settings work described below.

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

Books go in `/books/books`. Articles go in `/books/articles`. Older libraries with files directly inside `/books` are still read for compatibility, but the split folders are the recommended layout for `v0.0.4`.

If the device cannot see the SD card, the most common causes are:

- The card is exFAT instead of FAT32.
- The card has multiple partitions.
- The folders are missing or named differently.
- The card was removed without ejecting it from the computer.
- The card is slow, worn out, or unreliable.

The device includes an `SD card check` tool in the main menu to help diagnose card size, mount status, write access, and folder layout.

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

### Option 2: USB Transfer Mode

From the device:

1. Open the main menu with the `PWR` button.
2. Choose `USB transfer`.
3. Copy `.rsvp` files from your computer.
4. Eject the device from the computer.
5. Hold `PWR` to leave USB transfer mode.

Holding `PWR` is now the standard exit gesture for full-screen utility pages.

### Option 3: Web Companion

The device can host its own browser companion page.

1. Open the main menu.
2. Choose `Companion sync`.
3. The device shows the Wi-Fi network name and the browser URL.
4. Connect your phone, tablet, or computer to the `RSVP-Nano-xxxxxx` Wi-Fi network.
5. Open the URL shown on the device, usually `http://192.168.4.1`.

The web companion has pages for:

- `Books`: upload book `.rsvp` files and view the book library.
- `Articles`: write, paste, edit, preview, and upload articles.
- `Settings`: edit device settings and save home Wi-Fi credentials.
- `RSS`: manage RSS feed URLs.
- `Help`: quick notes for connection, conversion, SD cards, and RSS.

The web companion is the best option for Android, desktop, and anyone who does not have the iPhone app.

### Option 4: iPhone Companion App

The iPhone app supports companion sync, article editing, share-sheet saving, RSS feed management, device settings, and library progress.

TestFlight and App Store distribution are waiting on Apple Developer account approval. Until that is approved, the app can be installed temporarily from a Mac with Xcode.

See:

[`ios/RSVPNanoCompanion/README.md`](ios/RSVPNanoCompanion/README.md)

## Home Wi-Fi, RSS, And OTA

The device can save home Wi-Fi credentials for features that need internet access, such as RSS feed checks and OTA firmware updates.

You can set Wi-Fi credentials from:

- The web companion `Settings` page.
- The iPhone companion app settings page.
- The on-device Wi-Fi settings page.
- Advanced users can still use `/config/ota.conf`.

RSS feeds are managed from the web companion or the iPhone app, then checked from the device menu with `RSS feeds`. New articles are saved into `/books/articles`.

RSS support in `v0.0.4` includes:

- RSS and Atom feed parsing.
- Redirect handling for common `301`, `302`, `303`, `307`, and `308` responses.
- Live on-device progress while feeds are checked.
- Duplicate skipping.
- Feed item author, creator, or website name used as the article source.
- Larger feed downloads than earlier test builds.

Some feeds still block embedded clients, require JavaScript, return very large pages, or publish summaries instead of full articles. Those are feed or website limitations rather than SD card problems.

OTA updates use GitHub Releases. Open `Settings -> Firmware update` on the device after Wi-Fi is configured.

## Device Controls

### Hardware Buttons

- `PWR` short press from the reader: open the main menu.
- `PWR` short press on the main menu: return to the reader.
- `PWR` short press in most submenus: go back to the main menu.
- `PWR` hold in Companion Sync, USB Transfer, and Focus Timer: exit that page.
- `PWR` hold from the normal reader or main menu: power off.
- `BOOT` short press: cycle brightness.
- `BOOT` hold: cycle display theme.

The goal is simple: use `PWR` as menu, back, exit, and power. Use `BOOT` for quick display changes.

### Reader Controls

- Hold the screen: start reading.
- Release after a hold: pause.
- Double tap while paused: start locked continuous play.
- Tap while locked continuous play is running: pause.
- Tap the far-left edge: rewind to the start of the current sentence, or the previous sentence if you are already at the start.
- Swipe left or right while paused: scrub through nearby text.
- Tap after scrubbing: return to RSVP view.
- Hold and move vertically in the scrub preview: browse through surrounding text.
- Swipe up while paused: increase WPM.
- Swipe down while paused: decrease WPM.
- Tap the bottom-right footer label: switch between progress, chapter time remaining, book time remaining, and battery display modes.

Pause behavior is configurable. In `Settings -> Word pacing`, choose whether taps pause instantly or at the end of the sentence.

### Main Menu

Open the main menu with `PWR`.

```text
Resume
Chapters
Books
Articles
Focus Timer
Settings
SD card check
RSS feeds
Companion sync
USB transfer
Power off
```

Swipe up or down to move through the menu. Tap to select. Press `PWR` to go back.

### Books And Articles

`Books` shows files from `/books/books`.

`Articles` shows files from `/books/articles`.

Both pages show readable titles, progress, and saved position where available. Select an item to load it into the reader.

### Chapters

The `Chapters` page lists chapter markers from the current book when available. Select a chapter to jump to it. Press `PWR` to return to the main menu.

### Settings

Settings are grouped by how people actually use the device.

`Display` includes:

- Reading mode.
- Left/right handed layout.
- Display theme.
- Brightness.
- Footer and battery label behavior.
- Language.

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

- Long-word delay.
- Complexity delay.
- Punctuation delay.
- RSVP or scroll reading behavior.
- Instant pause or sentence-end pause.
- Pacing reset.

`Wi-Fi` includes network setup for RSS and OTA.

`Firmware update` checks GitHub Releases and installs newer firmware when available.

### Companion Sync

Use this page to connect the iPhone app or the web companion.

1. Open `Companion sync`.
2. Connect to the Wi-Fi network shown on the device.
3. Open the URL shown on the device.
4. Use the web companion or iPhone app.
5. Hold `PWR` to exit.

When Companion Sync exits, the device reloads settings and refreshes the library.

### USB Transfer

Use this page to copy files over USB without removing the SD card.

1. Open `USB transfer`.
2. Copy files from your computer.
3. Eject the device from the computer.
4. Hold `PWR` to exit.

Always eject before leaving USB transfer mode where possible.

### RSS Feeds

Use the web companion or iPhone app to manage feed URLs. Then run `RSS feeds` from the device menu.

The device shows live progress as it checks feeds. Saved articles appear in `Articles`.

### Focus Timer

The Focus Timer uses the device orientation to guide work and break blocks.

1. Open `Focus Timer`.
2. Choose a timer category.
3. Place or flip the device as prompted.
4. Follow the on-screen timer.
5. Hold `PWR` to exit the timer page.

Touch-and-hold during an active timer cancels the current timer block.

### SD Card Check

Run `SD card check` if books or articles do not appear. It checks whether the card mounts, whether it can write, and whether the expected library folders exist.

## Character Support

`v0.0.4` improves support for long books and unsupported characters. Common punctuation is normalized, and many accented Latin characters render directly or fall back to readable plain Latin equivalents.

The current renderer is best for English and European Latin-script languages. Complex scripts still need additional font and shaping work.

## iPhone App Status

The iPhone companion app is working, but public distribution is waiting on Apple Developer account approval.

Current app features include:

- Library view for books and articles.
- Article drafts, editing, preview, and sync.
- Safari and Chrome share flow.
- Fetch article title and text where available.
- Device settings editor.
- RSS feed management.
- Help and FAQ pages.

Temporary install instructions are in:

[`ios/RSVPNanoCompanion/README.md`](ios/RSVPNanoCompanion/README.md)

## Android App

The web companion works today from Android browsers.

A future native Android app is planned. Help from Android developers would be very welcome, especially around sharing URLs/articles into the app, local draft storage, and syncing with the device companion API.

## Build From Source

Firmware builds with PlatformIO:

```bash
pio run
```

Upload to a connected device:

```bash
pio run -t upload
```

Monitor serial output:

```bash
pio device monitor
```

The iPhone app lives in:

```text
ios/RSVPNanoCompanion
```

Open the Xcode project from that folder when installing the app locally.

To export browser-flasher and OTA firmware assets for a release:

```bash
python3 tools/export_web_firmware.py --version v0.0.4
```

That writes:

```text
web/firmware/rsvp-nano.bin
web/firmware/rsvp-nano-ota.bin
web/firmware/manifest.json
```

## Project Status

`v0.0.4` is the first release that brings the firmware, SD card workflow, RSS feeds, iPhone companion work, and web companion work together into one release-ready experience.

The next areas of work are:

- TestFlight and unlisted App Store distribution after Apple Developer approval.
- Better Android support, ideally with a native app.
- More capable article extraction for sites that do not expose full RSS content.
- A fuller browser-hosted companion experience for desktop and Android users.
- More font and script support.

## License

MIT. See [LICENSE](LICENSE).

The embedded OpenDyslexic and Atkinson Hyperlegible typeface assets are derived from the upstream projects and are included under the SIL Open Font License. See [third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and [third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
