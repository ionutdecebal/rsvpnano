> **0.2.0 candidate — publication pending.** Both firmware variants and the dictionary installer
> are implemented. The English dictionary uses a separate data release. Device acceptance,
> public firmware releases and the update catalog are still pending.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="companion/apps/web/src/wasmJsMain/composeResources/drawable/rsvp_nano_horizontal.svg">
    <img src="companion/apps/web/src/wasmJsMain/composeResources/drawable/rsvp_nano_horizontal_light.svg" alt="RSVP Nano" width="520">
  </picture>
</p>

<p align="center">
  An open-source, pocket-sized speed reader built on ESP32.
</p>

<p align="center">
  <a href="https://github.com/ionutdecebal/rsvpnano/releases"><img alt="Classic stable release" src="https://img.shields.io/github/v/release/ionutdecebal/rsvpnano?sort=semver&label=Classic"></a>
  <a href="https://github.com/ionutdecebal/rsvpnano/actions/workflows/test.yml"><img alt="Tests" src="https://github.com/ionutdecebal/rsvpnano/actions/workflows/test.yml/badge.svg"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/ionutdecebal/rsvpnano"></a>
  <a href="https://discord.gg/mB5xv2PG53"><img alt="Discord" src="https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white"></a>
</p>

<p align="center">
  <a href="https://ionutdecebal.github.io/rsvpnano/"><strong>Open the web companion</strong></a>
  &nbsp;|&nbsp;
  <a href="https://github.com/ionutdecebal/rsvpnano/releases">Releases</a>
  &nbsp;|&nbsp;
  <a href="#choose-your-software">Choose your software</a>
  &nbsp;|&nbsp;
  <a href="#rsvp-nano-and-pocketlines">About Pocketlines</a>
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

RSVP Nano is a community-powered project: open source, experimental, and built for people who
like making a reader their own. Books and reading progress stay on your device. The current
release line is `0.x`; back up important SD card content before testing new firmware.

## Choose your software

Two firmware choices, one RSVP Nano project. Both keep your books on the SD card and work with
the existing web companion.

| | RSVP Nano 0.2.0 · New UI | RSVP Nano · Classic |
| --- | --- | --- |
| Interface | Redesigned reading screen, consolidated menus, larger touch targets, and full-area sliders | The existing RSVP Nano interface and controls |
| Reading playback | Single words or centred contextual chunks, with adjustable pacing | Single-word RSVP and page reading modes |
| Finding your place | Scroll through pages, search the book, and jump between chapters; page view is for browsing | Existing page reading and book navigation |
| Quick controls | Middle button opens speed, brightness, menu access, and Dictionary | Existing controls for the selected board |
| Getting started | On-device introduction covering buttons, speed and pacing, SD checks, book uploads, and dictionary use | Existing web companion setup |
| Focus timer | Background reading sessions or standalone timers, presets, full-screen breaks, and manual reading resume | Existing configurable focus timers and orientation controls |
| Settings | Detailed settings in consolidated menus; per-book font and language controls beside each Library book | Existing detailed reader, appearance, and per-book settings |
| Appearance | Themes, reader fonts, tracking, positioning, and reading guides; guides are off in chunk mode | Themes, reader fonts, tracking, focus highlighting, and configurable guide geometry |
| Dictionary | Offline English definitions plus optional direct Wi-Fi lookup; no account or companion server needed | Not included |
| Books and connections | Existing web companion, USB transfers, and Wi-Fi connections | Existing web companion, USB transfers, and Wi-Fi connections |
| Hardware | **Waveshare ESP32-S3 Touch LCD 3.49 Rev1 only** for this release | [Existing supported boards](#supported-hardware) |
| Release channel | **0.2.0 preview**, explicitly selected | Stable; remains the default for existing installations |
| Firmware updates | Update the New UI or select **Return to Classic** | Selector maintenance build: update Classic or select **Try New UI** on compatible hardware |
| Source | [`release/ui-dictionary`](https://github.com/ionutdecebal/rsvpnano/tree/release/ui-dictionary) | [`main`](https://github.com/ionutdecebal/rsvpnano/tree/main) |

**The choice stays yours.** Updating your current software should keep you on that interface;
switching to the other one is an explicit choice. It installs the selected firmware and restarts
the reader. See [Updates and switching](#updates-and-switching) for the on-device flow.

**Release scope:** 0.2.0 includes the New UI, onboarding, and dictionary alongside the existing
reader functions. Voice notes, transcription, Bluetooth phone sync, and the experimental new
phone app are being developed separately and are not included in this release.

[Open the web installer](https://ionutdecebal.github.io/rsvpnano/) ·
[Browse releases](https://github.com/ionutdecebal/rsvpnano/releases)

## RSVP Nano and Pocketlines

**Pocketlines is a new product we're building alongside RSVP Nano**, for people who want a reader
they can buy and start using straight away. It will bring together preinstalled software, a simpler
on-device experience, and services that don't require technical setup or self-hosting.

We're designing a dedicated Pocketlines device with an **OLED screen, more physical buttons, and
a metal casing**. It's still early in development; these are plans for the upcoming device.

**RSVP Nano will stay free and actively developed** as the experimental, DIY and self-hosted
foundation. We'll keep developing software for the existing Waveshare boards, with detailed
settings and room for community experimentation. RSVP Nano 0.2.0 is part of that continued work.

The software for **both projects will remain open source**, maintained in separate repositories.
Our commitment is that **every Pocketlines software feature will have a DIY or self-hosted
equivalent in RSVP Nano**. Pocketlines focuses on making the experience ready to use; Nano gives
you the tools and flexibility to build and run it yourself.

Community feedback helps shape both projects. Share what you enjoy, what gets in your way, and
what you'd like to see next in the [RSVP Nano and Pocketlines Discord](https://discord.gg/mB5xv2PG53).

## Get started

You need a supported RSVP Nano board, a USB-C data cable, and a FAT32 microSD card. An 8 GB to
32 GB card is the safest choice.

1. Open the [web companion](https://ionutdecebal.github.io/rsvpnano/) in a desktop browser.
2. Choose your board and firmware: **Classic** is the default. **New UI (preview) · 0.2.0**
   is available for LCD 3.49 Rev1.
3. Follow setup and verify the reader. The New UI includes an on-device introduction.
4. Add an EPUB, RSVP, text, Markdown, HTML, or XHTML file from the companion's Library screen.
5. Open the book on the Nano and start reading. For offline definitions in the New UI,
   [install the dictionary pack](#dictionary-setup-new-ui).

Wi-Fi is optional for reading and offline dictionary use. Use USB to transfer books if you
prefer; the experimental new phone app is not required.

Chrome or Edge is required for browser flashing and USB companion access because those features use
Web Serial. The site still supports local-network management in browsers that can reach the reader.

The web installer keeps Classic and New UI downloads separate. You can also select a compatible
merged `.bin` file manually. Use the application-only `-ota.bin` for an OTA update, not the merged
image. Always match the firmware to your board revision.

## Updates and switching

On a compatible reader, **Firmware updates** offers:

- **Update current software** — get updates for the interface you already use.
- **Return to Classic** from the New UI, or **Try New UI** from Classic.

The reader shows the installed version and the selected version before you confirm.
It downloads the correct firmware over Wi-Fi, show progress, and restart when installation
is complete. Switching is always intentional. Custom repository/tag settings remain available for normal
updates. An explicit interface switch backs up that advanced source in device storage and uses
the official channel; it does not automatically restore a custom source later.

The selector is also included in a small Classic maintenance build so you can switch in both directions.
Older Classic builds can use their existing **Release tag** setting to select `v0.2.0` after
publication, or use the web installer. New UI choices will only appear for supported hardware.

Classic and the New UI have separate update selections. The New UI can remain a GitHub
prerelease while still being offered in the device's update menu.

Back up the SD card, including settings and reading progress, before switching. Both firmwares use
the existing library layout, but Classic may ignore new settings or replace them when saving.
Keep the backup to restore your exact New UI preferences. Switching must not format the SD card.

The web installer remains an alternative: choose the firmware for the same board and leave
**Erase first** off. Use the web companion's installer; a generic whole-chip erase can remove
saved device settings.

## Dictionary setup (New UI)

The dictionary lives on the SD card, separately from the firmware, so you can keep or replace it
without reflashing the reader. English is available now; more languages are planned.

### Install from the reader

During onboarding, or from **Dictionary → ? → Install / manage**, choose
**Install English dictionary · about 48 MiB**. The reader will check the SD card and available
space, connect over Wi-Fi, then download and verify the pack with visible progress.
You can skip this during setup and install it later. Dictionary packs are hosted on GitHub
and versioned separately from the firmware.

An existing dictionary stays available until a replacement has downloaded and passed
verification. Replacing a manually installed pack requires an explicit choice. Press the middle
button to cancel a download; retry starts a fresh transfer.

### Install manually or use a custom pack

1. Download `RSVP-Nano-English-Dictionary.zip` from the
   [English dictionary release](https://github.com/ionutdecebal/rsvpnano/releases/tag/dictionary-en-2025.1).
2. Extract it and copy the `dictionaries` folder to the root of the SD card using a card reader
   or USB mass-storage mode.
3. From a book, press the **middle button → Dictionary**. Use **Search** to look up another word.
   The **?** button inside Dictionary opens setup help.

The complete release ZIP includes the same dictionary folder alongside the firmware
and installation instructions. Downloading that ZIP to a computer does not automatically copy
the dictionary to the reader's SD card.

The resulting files should be:

```text
/dictionaries/english.oewn
/dictionaries/ATTRIBUTION.txt
/dictionaries/manifest.json
```

The pack uses about **48 MiB** and contains English definitions. It works without Wi-Fi, a phone,
an account, or a server. It does not install interface translations.

For a custom pack, use the compatible indexed format expected at `/dictionaries/english.oewn`.
The [dictionary build tool](https://github.com/ionutdecebal/rsvpnano/tree/release/ui-dictionary/tools/dictionary)
documents the current Open English Wordnet conversion. Arbitrary dictionary files are not
automatically compatible; additional formats and dictionary selection would need further support.

**Look online** makes a request directly to Wiktionary over configured Wi-Fi when you choose it.
The lookup word is sent, not the book. Offline lookup remains available when an online request fails.

The offline pack adapts [Open English Wordnet 2025](https://en-word.net/), by the Open English
Wordnet Community and derived from Princeton WordNet, under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). It includes compact definitions,
examples, form aliases, and up to eight senses per entry. Keep its attribution files when
redistributing it. Online definitions come from Wiktionary contributors under
[CC BY-SA 4.0](https://en.wiktionary.org/wiki/Wiktionary:Copyrights).

## Supported hardware

**The table below describes Classic.** New UI 0.2.0 is offered only for
**Waveshare ESP32-S3 Touch LCD 3.49 Rev1**. Use Classic on the other boards.

| Board | PlatformIO environment | Distribution |
| --- | --- | --- |
| Waveshare ESP32-S3 Touch LCD 3.49 rev1 | `waveshare_esp32s3_touch_lcd_349_rev1` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch LCD 3.49 rev2 | `waveshare_esp32s3_touch_lcd_349_rev2` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 1.8 V1 | `waveshare_esp32s3_touch_amoled_18_v1` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 1.8 V2 | `waveshare_esp32s3_touch_amoled_18_v2` | Web installer, OTA, source; still experimental |
| Waveshare ESP32-S3 Touch AMOLED 2.06 | `waveshare_esp32s3_touch_amoled_206` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 2.16 | `waveshare_esp32s3_touch_amoled_216` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 2.41 V1 | `waveshare_esp32s3_touch_amoled_241` | Web installer, OTA, source |
| Waveshare ESP32-S3 Touch AMOLED 2.41 V2 | `waveshare_esp32s3_touch_amoled_241_v2` | Web installer, OTA, source; hardware validation pending |
| Waveshare ESP32-C6 Touch LCD 1.47 | `waveshare_esp32c6_touch_lcd_147` | Web installer, OTA, source |

Most LCD 3.49 readers use rev1. Try rev2 when the display works but backlight control does not.

For AMOLED 2.41, choose V2 only for a Rev2.0 PCB or V2 QC label. Reset/interrupt wiring
is different; V1 and V2 firmware are not interchangeable. Both use the regular UI.

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
| [Android](companion/apps/android/README.md) | Jetpack Compose | Build from source |
| [iOS](companion/apps/ios/RSVPNanoCompanion/README.md) | SwiftUI | Build from source |

The companions share device models, API contracts, conversion, persistence formats, and workflows.
Their layouts remain platform-specific. These are the existing companions; the experimental
new phone app with voice notes and Bluetooth sync is not part of New UI 0.2.0.

To connect over Wi-Fi, open Companion Sync on the Nano. The apps prefer a reader discovered on the
same local network and can fall back to the Nano's direct `RSVP-Nano-xxxxxx` network or a manual
address. The web companion can also connect directly over USB on supported desktop browsers.

## Library and SD card

The firmware creates its standard folders when writable storage is available. The main layout is:

```text
/library/books
/library/articles
/config
/fonts
/locales
/themes
```

Older cards using `/books` are migrated to `/library` without overwriting existing files. The same repair and validation pass is available from the Nano's Device screen and from the web or mobile companion while connected.

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

The default `main` branch builds Classic. For the exact New UI 0.2.0 release source:

```bash
git fetch --tags
git switch --detach v0.2.0
git submodule update --init --recursive
```

Ongoing New UI development lives on `release/ui-dictionary`. Its first release target is
LCD 3.49 Rev1.

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
[Android](companion/apps/android/README.md) and
[iOS](companion/apps/ios/RSVPNanoCompanion/README.md) guides.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Firmware application, board ports, reading engine, UI, storage, networking, and companion API |
| `companion/shared/` | Shared companion models, API clients, persistence, and workflows |
| `companion/conversion/` | Shared Kotlin Multiplatform document conversion |
| `companion/apps/` | Android, iOS, and web applications |
| `fonts/`, `themes/`, `locale-packs/` | Installable catalogs |
| `localization/` | Locale and UI-font generation |
| `test/` | Native firmware tests |
| `docs/` | Protocol, configuration, hardware, and release documentation |

## Documentation

- [Classic releases](https://github.com/ionutdecebal/rsvpnano/releases/latest)
- [New UI 0.2.0 release notes and downloads](https://github.com/ionutdecebal/rsvpnano/releases/tag/v0.2.0)
- [Companion API](docs/companion-api.md)
- [USB companion protocol](docs/companion-serial.md)
- [Conversion format and behavior](docs/conversion/spec.md)
- [Configuration files](docs/configuration.md)
- [Hardware architecture and bring-up notes](docs/hardware/README.md)
- [Reader fonts](fonts/README.md)
- [Themes](themes/README.md)
- [Locale packs](locale-packs/README.md)

## Contributing

Issues and focused pull requests are welcome. For bug reports, include **Classic or New UI**,
the firmware version, board revision, and steps to reproduce. Target firmware changes to the
appropriate branch: `main` for Classic, `release/ui-dictionary` for New UI.

Before opening a change:

1. Build the affected firmware or companion target.
2. Run the smallest relevant test suite.
3. Keep board-specific behavior inside its platform directory.
4. Reuse shared companion and conversion logic instead of creating another platform implementation.
5. Include hardware validation details for changes that affect display, touch, power, storage, or USB.

For questions, hardware reports, and development discussion, join the
[RSVP Nano Discord](https://discord.gg/mB5xv2PG53).

## License

RSVP Nano is distributed under the [MIT license](LICENSE). Bundled fonts and third-party
libraries retain their own licenses in their respective directories. Dictionary data has its
own attribution and licenses, described in [Dictionary setup](#dictionary-setup-new-ui).
