# Copilot Instructions — rsvpnano (noogi fork)

## Repository context

This is a **fork** of [ionutdecebal/rsvpnano](https://github.com/ionutdecebal/rsvpnano), maintained by **giovannimazza** as `release/rsvp-noogi`. Upstream `main` is kept clean; all custom features live on `release/rsvp-noogi` and individual `hot-feature/*` branches.

---

## Build & test

**Build firmware (PlatformIO):**
```sh
pio run -e waveshare_esp32s3_usb_msc
```

**Run native unit tests (pacing logic only):**
```sh
pio test -e native_test
```
Tests live in `test/` and use the Unity framework. The only test suite is `test_pacing`.

**Release a new version (Windows, from `release/rsvp-noogi`):**
```powershell
.\tools\release.ps1 -Version v0.1.X-noogi
```
This creates an annotated tag and pushes it, which triggers `.github/workflows/release.yml` to build and publish the firmware. Version must match `v\d+\.\d+\.\d+` (suffix like `-noogi` is allowed).

**Firmware version macro:** injected at build time by `tools/pio_set_version.py` as `RSVP_FIRMWARE_VERSION`. Available via `firmwareVersionLabel()` in `App.cpp`.

---

## Architecture

The firmware is a single-threaded Arduino loop on ESP32-S3. Entry point is `src/main.cpp` → `App::begin()` + `App::update()`.

### Module overview

| Module | Purpose |
|---|---|
| `src/app/App.cpp/.h` | Central orchestrator — all state machines, settings, touch handling, menu navigation |
| `src/display/DisplayManager` | All rendering: RSVP word, scroll view, menus, settings, footer chrome |
| `src/reader/ReadingLoop` | Word timing engine — WPM, pacing delays, scrub, seek |
| `src/storage/StorageManager` | SD card, book loading, `.rsvp` index format, chapter markers |
| `src/storage/EpubConverter` | EPUB → `.rsvp` conversion on-device |
| `src/timer/FocusTimer` | Focus timer with per-genre duration memory |
| `src/update/OtaUpdater` | OTA firmware updates via GitHub Releases |
| `src/sync/CompanionSyncManager` | Browser/iOS companion sync over WiFi |

### Settings persistence

All user preferences are stored in ESP32 NVS via `Preferences`. Keys are defined as `constexpr const char *kPrefXxx` at the top of `App.cpp`. **NVS keys must be ≤ 15 characters.**

### Settings menu structure

Settings menus are driven by index constants (`kSettingsXxxIndex`) + a `rebuildSettingsMenuItems()` builder + `selectSettingsItem()` handler. When adding a new setting:
1. Add a `constexpr size_t kSettingsXxxIndex = N;` constant
2. Add a `push_back(...)` in `rebuildSettingsMenuItems()` in the correct menu block
3. Add a `case kSettingsXxxIndex:` in the Display/Pacing handler inside `selectSettingsItem()`
4. Add the NVS key constant and load it in the boot pref-loading block (after `readerMode_` is loaded if it depends on the reader mode)

### Reader modes

`App::ReaderMode::Rsvp` (default) and `App::ReaderMode::Scroll`. `scrollModeEnabled()` returns `readerMode_ == ReaderMode::Scroll`. Chapter label visibility and other per-mode defaults use separate NVS keys (`kPrefChapterLabelRsvp`, `kPrefChapterLabelScroll`).

### Chapter labels

`currentChapterLabel()` → `cleanedChapterTitle()` strips leading `N.` prefixes from EPUB spine IDs. If no heading was found during EPUB conversion, the raw spine filename is used as title — this is expected behavior.

### `readerChrome()`

Controls footer visibility flags (`showBattery`, `showChapter`, `showProgress`, `showPreviousSentenceHint`). Chapter label is hidden when `!chapterLabelEnabled_ || scrollModeEnabled()`.

---

## Branch conventions

| Branch | Purpose |
|---|---|
| `main` | Upstream (ionutdecebal) — do not add noogi features here |
| `release/rsvp-noogi` | Integration branch — all noogi features merged here |
| `hot-feature/*` | Feature branches — **must be cut from `main`**, contain only their specific feature vs main, then merged into `release/rsvp-noogi` |

**Do not** cherry-pick or merge `release/rsvp-noogi` back into a `hot-feature/*` branch — this contaminates it with other features. Use cherry-pick from specific commits if needed.

**Commits:** no `Co-authored-by: Copilot` trailer (user preference).

---

## OTA / fork config

`src/update/OtaUpdater.h` default `githubOwner` must remain `"giovannimazza"` (not upstream `"ionutdecebal"`). This controls where OTA checks for updates.

---

## Key build flags

| Flag | Meaning |
|---|---|
| `RSVP_ON_DEVICE_EPUB_CONVERSION=1` | Enable on-device EPUB → .rsvp conversion |
| `RSVP_USB_TRANSFER_ENABLED=1` | Enable USB MSC mode (drag-and-drop) |
| `RSVP_FIRMWARE_VERSION` | Injected at build time from git tag |
