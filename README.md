<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="RSVPNanoCompanion/apps/web/src/wasmJsMain/composeResources/drawable/rsvp_nano_horizontal.svg">
    <img src="RSVPNanoCompanion/apps/web/src/wasmJsMain/composeResources/drawable/rsvp_nano_horizontal_light.svg" alt="RSVP Nano" width="520">
  </picture>
</p>

<p align="center">
  An open-source, pocket-sized speed reader built on ESP32.
</p>

<p align="center">
  <a href="https://github.com/ionutdecebal/rsvpnano/releases"><img alt="Latest release" src="https://img.shields.io/github/v/release/ionutdecebal/rsvpnano?sort=semver"></a>
  <a href="https://github.com/ionutdecebal/rsvpnano/actions/workflows/test.yml"><img alt="Tests" src="https://github.com/ionutdecebal/rsvpnano/actions/workflows/test.yml/badge.svg"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/ionutdecebal/rsvpnano"></a>
  <a href="https://discord.gg/mB5xv2PG53"><img alt="Discord" src="https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white"></a>
</p>

<p align="center">
  <a href="https://ionutdecebal.github.io/rsvpnano/"><strong>Open the web companion</strong></a>
  &nbsp;|&nbsp;
  <a href="https://github.com/ionutdecebal/rsvpnano/releases">Releases</a>
  &nbsp;|&nbsp;
  <a href="docs/releases/v0.0.9.md">v0.0.9 notes</a>
  &nbsp;|&nbsp;
  <a href="https://discord.gg/mB5xv2PG53">Discord</a>
</p>

## What is RSVP Nano?

RSVP Nano is a small standalone reader that presents text one word at a time using Rapid Serial
Visual Presentation. It also supports page reading when you want surrounding context. Books,
articles, settings, fonts, themes, languages, and reading progress live on a microSD card.

The project includes:

- Multi-board ESP32 firmware with RSVP and page reading.
- A responsive Compose Multiplatform web companion for setup, flashing, conversion, and management.
- Android and iOS companions built on shared Kotlin Multiplatform logic.
- A shared Kotlin conversion engine for EPUB, text, Markdown, HTML, XHTML, and RSVP files.
- Installable reader fonts, interface language packs, and color themes.

RSVP Nano is under active development. The current release line is `0.x`, so back up important SD
card content before testing new firmware.

## Highlights

- **Two reading modes:** focused RSVP playback and paragraph-aware page reading.
- **Multilingual text:** bidirectional text, Arabic and Hebrew shaping, CJK fonts, and vertical CJK
  books.
- **Personal appearance:** installable RFont4 reader fonts, locale packs, downloadable themes,
  focus highlighting, tracking, guide geometry, and handed layouts.
- **Companion management:** library uploads, progress editing, settings, Wi-Fi, RSS feeds, focus
  timers, fonts, themes, locales, and firmware updates.
- **Flexible connections:** local-network sync, the Nano's direct Wi-Fi network, USB companion
  control, and USB mass-storage transfer on supported boards.
- **Offline-friendly setup:** an installable PWA shell, browser firmware installation, Improv Wi-Fi
  provisioning, and local file conversion without a server-side converter.
- **Device utilities:** OTA updates, SD diagnostics, configurable focus routines, and Life, Maze,
  Voronoi, and Reaction screensavers.

## Get started

You need a supported RSVP Nano board, a USB-C data cable, and a FAT32 microSD card. An 8 GB to
32 GB card is the safest choice.

1. Open the [web companion](https://ionutdecebal.github.io/rsvpnano/) in a desktop browser.
2. Choose your board and install the latest firmware.
3. Follow setup to provision Wi-Fi and verify the reader.
4. Add an EPUB, RSVP, text, Markdown, HTML, or XHTML file from the Library screen.
5. Open the book on the Nano and start reading.

Chrome or Edge is required for browser flashing and USB companion access because those features use
Web Serial. The site still supports local-network management in browsers that can reach the reader.

The web installer uses the firmware bundled with the latest published release. You can also choose a
compatible full-image `.bin` file manually. Firmware already installed on a Nano can update through
the device's OTA flow.

## Supported hardware

| Board | PlatformIO environment | Distribution |
| --- | --- | --- |
| Waveshare ESP32-S3 Touch LCD 3.49 rev1 | `waveshare_esp32s3_touch_lcd_349_rev1` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch LCD 3.49 rev2 | `waveshare_esp32s3_touch_lcd_349_rev2` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 1.8 V1 | `waveshare_esp32s3_touch_amoled_18_v1` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 1.8 V2 | `waveshare_esp32s3_touch_amoled_18_v2` | Web installer, OTA, source; still experimental |
| Waveshare ESP32-S3 Touch AMOLED 2.06 | `waveshare_esp32s3_touch_amoled_206` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 2.16 | `waveshare_esp32s3_touch_amoled_216` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 2.41 | `waveshare_esp32s3_touch_amoled_241` | Web installer, OTA, source |
| Waveshare ESP32-C6 Touch LCD 1.47 | `waveshare_esp32c6_touch_lcd_147` | Web installer, OTA, source |

Most LCD 3.49 readers use rev1. Try rev2 when the display works but backlight control does not.

The following purchase links are affiliate links. A purchase may support RSVP Nano at no extra cost
to you:

- [Touch LCD 3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=ionutdecebal)
- [Touch AMOLED 1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm?&aff_id=ionutdecebal)
- [Touch AMOLED 2.06](https://www.waveshare.com/esp32-s3-touch-amoled-2.06.htm?&aff_id=ionutdecebal)
- [Touch AMOLED 2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=ionutdecebal)
- [Touch AMOLED 2.41](https://www.waveshare.com/esp32-s3-touch-amoled-2.41.htm?&aff_id=ionutdecebal)
- [ESP32-C6 Touch LCD 1.47](https://www.waveshare.com/esp32-c6-touch-lcd-1.47.htm?&aff_id=ionutdecebal)

## Companion apps

| Companion | UI | Status |
| --- | --- | --- |
| [Web](https://ionutdecebal.github.io/rsvpnano/) | Compose Multiplatform on Kotlin/Wasm | Hosted setup, flashing, conversion, USB, and LAN management |
| [Android](RSVPNanoCompanion/apps/android/README.md) | Jetpack Compose | Build from source |
| [iOS](RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/README.md) | SwiftUI | Build from source |

The companions share device models, API contracts, conversion, persistence formats, and workflows.
Their layouts remain platform-specific.

To connect over Wi-Fi, open Companion Sync on the Nano. The apps prefer a reader discovered on the
same local network and can fall back to the Nano's direct `RSVP-Nano-xxxxxx` network or a manual
address. The web companion can also connect directly over USB on supported desktop browsers.

## Library and SD card

The firmware creates its standard folders when writable storage is available. The main layout is:

```text
/books/books
/books/articles
/config
/fonts
/locales
/themes
```

Books and articles may be uploaded by a companion or copied directly to the card. The reader creates
rebuildable `.ridx` and `.rdat` index files plus hidden `.rstate.toml` files for durable progress and
per-book typography.

Always eject USB mass storage before disconnecting it. Run the on-device SD card check if the
library is missing, read-only, or using an unexpected filesystem layout.

Configuration is stored as versionless TOML. See [Configuration](docs/configuration.md) for settings,
RSS, focus timers, themes, NVS mirroring, and recovery behavior.

## Build from source

Clone submodules first:

```bash
git clone --recurse-submodules https://github.com/ionutdecebal/rsvpnano.git
cd rsvpnano
```

### Firmware

Install [PlatformIO](https://platformio.org/), then build one target:

```bash
pio run -e waveshare_esp32s3_touch_lcd_349_rev1
```

Upload to a connected board:

```bash
pio run -e waveshare_esp32s3_touch_lcd_349_rev1 -t upload
```

Run native firmware tests:

```bash
pio test -e native_test
```

### Web and mobile companions

Companion builds require JDK 17. Android also requires the Android SDK; iOS requires macOS and Xcode.

```bash
# Browser tests and staged Pages distribution
./gradlew checkWeb

# Shared tests and Android release build
./gradlew checkAndroid

# Shared iOS compilation and simulator tests
./gradlew checkIos
```

The staged website is written to `build/webSite`. Detailed native setup is in the
[Android](RSVPNanoCompanion/apps/android/README.md) and
[iOS](RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/README.md) guides.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Firmware application, board ports, reading engine, UI, storage, networking, and companion API |
| `RSVPNanoCompanion/shared/` | Shared companion models, API clients, persistence, and workflows |
| `RSVPNanoCompanion/conversionCore/` | Shared Kotlin Multiplatform document conversion |
| `RSVPNanoCompanion/apps/` | Android, iOS, and web applications |
| `fonts/`, `themes/`, `locale-packs/` | Installable catalogs |
| `localization/` | Locale and UI-font generation |
| `test/` | Native firmware tests |
| `docs/` | Protocol, configuration, hardware, and release documentation |

## Documentation

- [v0.0.9 release notes](docs/releases/v0.0.9.md)
- [Companion API](docs/companion-api.md)
- [USB companion protocol](docs/companion-serial.md)
- [Conversion format and behavior](RSVPNanoCompanion/docs/conversion-spec.md)
- [Configuration files](docs/configuration.md)
- [Hardware architecture and bring-up notes](docs/hardware/README.md)
- [Reader fonts](fonts/README.md)
- [Themes](themes/README.md)
- [Locale packs](locale-packs/README.md)

## Contributing

Issues and focused pull requests are welcome. Before opening a change:

1. Build the affected firmware or companion target.
2. Run the smallest relevant test suite.
3. Keep board-specific behavior inside its platform directory.
4. Reuse shared companion and conversion logic instead of creating another platform implementation.
5. Include hardware validation details for changes that affect display, touch, power, storage, or USB.

For questions, hardware reports, and development discussion, join the
[RSVP Nano Discord](https://discord.gg/mB5xv2PG53).

## License

RSVP Nano is distributed under the [MIT license](LICENSE). Bundled fonts and third-party
libraries retain their own licenses in their respective directories.
