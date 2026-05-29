# RSVP Nano

RSVP Nano is an open-source ESP32-S3 reading device that shows text one word at a time using RSVP, Rapid Serial Visual Presentation. It is designed for small screens, SD card libraries, fast reading, and a simple browser-first workflow for converting and uploading books.

This README is written for the current release, `v0.1.166-noogi`.

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

The hosted flasher installs the latest published GitHub Release. For `v0.0.5`, that means the release build includes the firmware, SD card, RSS, companion sync, web companion, and settings work described below.

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

Books go in `/books/books`. Articles go in `/books/articles`. Older libraries with files directly inside `/books` are still read for compatibility, but the split folders are the recommended layout for `v0.0.5`.

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

On first open, the firmware may create `.ridx` and `.rdat` sidecar files next to a book. These are the SD-backed word index and normalized word data used for long books. Leave them on the card; they are rebuilt automatically if the source book changes.

Large books now load through the same indexed reading path as smaller books, with progress messages while indexes and time estimates are prepared. If a book cannot be prepared, the device should return to the menu with a readable reason instead of silently failing.

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

RSS support in `v0.0.5` includes:

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
- `PWR` hold from the normal reader or main menu: power-off confirmation dialog. Confirm with `PWR` again or wait 1.6 seconds to proceed; any other action cancels.
- `BOOT` short press: cycle brightness (with live on-screen toast showing current level).
- `BOOT` hold: cycle display theme.
- Press `PWR` + `BOOT` together: enter standby. Press either button to wake after the short standby grace period.

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
- Tap the top-right battery label: switch between percentage, time remaining, and voltage.

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

- Reading mode (RSVP or Scroll).
- Left/right handed layout.
- Display theme.
- Brightness with live toast feedback.
- Chapter label visibility toggle (independent per reading mode).
- Footer and battery label behavior.
- Optional battery, chapter, and book percentage labels while actively reading.
- Standby display mode: Life, Maze, Voronoi, or Screen off.
- Language.

`Battery` includes:

- **CPU RSVP mode**: CPU frequency for RSVP word-by-word reading (80/160/240 MHz). Default: 160 MHz.
- **CPU scroll mode**: CPU frequency for scroll-mode active reading (80/160/240 MHz). Default: 160 MHz.
- **CPU paused**: CPU frequency for pause state in RSVP mode (80/160/240 MHz). Default: 80 MHz. Affects manual scroll speed in RSVP.
- **CPU menu**: CPU frequency for menu navigation (80/160/240 MHz). Default: 80 MHz.
- **CPU standby**: CPU frequency for standby/screensaver (40/80/160/240 MHz). Default: 80 MHz. 40 MHz may affect screensaver animations.
- **Auto-dim delay**: Off / 30s / 60s / 2min. Automatically reduces screen brightness after user inactivity.
- **Auto-dim brightness level**: 0% (Screen off) / 10% / 20% / 30%. Brightness level applied when auto-dim activates.

Battery estimate updates instantly on reading-mode change and reflects the actual CPU frequency mix used. Nominal runtime: 450 minutes at 160 MHz, adjusted by selected CPU frequencies and OTA settings.

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
- Reading speeds from 10 WPM upward, with 10 WPM steps below 100 WPM.
- Instant pause or sentence-end pause.
- Pacing reset.

`Wi-Fi` includes network setup for RSS and OTA.

`Firmware update` checks GitHub Releases and installs newer firmware when available. Displays current firmware version and build timestamp.

`About` displays device information including firmware version, hardware platform, and uptime.

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

If a feed cannot be downloaded, the reader shows a plain-English reason such as `Feed not found`, `Site blocked reader`, or `Site took too long`.

RSS checks can continue in the background, while installable firmware updates still ask for confirmation before the device changes itself.

### Focus Timer

The Focus Timer uses the device orientation to guide work and break blocks.

1. Open `Focus Timer`.
2. Choose a timer category (configurable duration per category).
3. Place or flip the device as prompted.
4. Follow the on-screen timer.
5. Hold `PWR` to exit the timer page.

Touch-and-hold during an active timer cancels the current timer block. Timer settings are saved per genre/category.

### SD Card Check

Run `SD card check` if books or articles do not appear. It checks whether the card mounts, whether it can write, and whether the expected library folders exist.

If the old folder layout needs repair, the device now asks before changing the card.

## Character Support

`v0.0.5` improves support for long books and unsupported characters. Common punctuation is normalized, ellipses and hyphenated sentence breaks are handled more carefully, and many accented Latin characters render directly or fall back to readable plain Latin equivalents.

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
python3 tools/export_web_firmware.py --version v0.0.5
```

That writes:

```text
web/firmware/rsvp-nano.bin
web/firmware/rsvp-nano-ota.bin
web/firmware/manifest.json
```

## Project Status

`v0.1.166-noogi` adds comprehensive battery optimization and power management:

**Battery and Power:**
- **Per-context CPU scaling**: 5 configurable CPU frequencies (RSVP mode, Scroll mode, Paused, Menu, Standby) to balance performance and power consumption.
- **Auto-dim**: Automatic brightness reduction after inactivity, with user-selectable delay and brightness level (0% to fully power off the backlight).
- **Accurate battery estimate**: Real-time runtime prediction that reflects the actual CPU mix in use, updates instantly on mode change.
- **Standby low-power option**: 40 MHz option for extended standby life (may affect screensaver animation smoothness).
- **Backlight PWM fix**: Proper voltage clamping to prevent screen dimming below 35% brightness threshold.
- **Power-off confirmation**: 1.6s hold on PWR button shows a confirmation dialog to prevent accidental shutdown.
- **Scroll-mode CPU awareness**: CPU settings for each reading mode are tracked independently; changing Scroll CPU in RSVP mode has no effect on battery estimate.

**Display and UI:**
- **Brightness live feedback**: Toast notification shows current brightness percentage when adjusted via `BOOT` button.
- **Firmware version in Settings**: `About` page displays current firmware version, build timestamp, and device uptime.
- **Chapter label management**: Per-reading-mode toggle for chapter label visibility in footer chrome.

**Focus Timer:**
- Configurable timer duration per category/genre (settings persist across sessions).

Earlier `v0.1.x-noogi` releases introduced the battery optimization framework, fixed settings menu scrolling, and added these granular controls.

The next areas of work are:

- TestFlight and unlisted App Store distribution after Apple Developer approval.
- Better Android support, ideally with a native app.
- More capable article extraction for sites that do not expose full RSS content.
- A fuller browser-hosted companion experience for desktop and Android users.
- More font and script support.

## License

MIT. See [LICENSE](LICENSE).

The embedded OpenDyslexic and Atkinson Hyperlegible typeface assets are derived from the upstream projects and are included under the SIL Open Font License. See [third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and [third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
