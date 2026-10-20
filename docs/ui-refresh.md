# Historical UI notes

These notes describe earlier September prototypes, not the current candidate.
For current scope, installation and validation, see [the UI + dictionary guide](releases/ui-dictionary-preview.md).

# RSVP Nano — Preview Edition

Branch: `feat/reader-first-ui`  
Baseline: upstream `f9d2a800ed38ce9ff64c6711e8ef2eca5db699fb`  
Target: Waveshare LCD 3.49, 640 × 172 landscape

This is the fuller native UI variant of the approved `sketches/firmware-study-03` direction. The HTML sketch is not embedded in the device. The earlier reader-first flash package remains separate and unchanged.

## What changed

- Book-first Menu with saved progress, direct Chapters/Library/Settings access and persisted brightness controls.
- Title-led Library, two-line titles, large previous/next arrows, book details and an Add route to the existing connection flows. Arrows wrap; only Open opens a book.
- Thin icon-row Settings and Device hubs; new Reading pace, Text & display, Connections, Storage and Sleep & power pages. Detailed brightness, language, sleep, pacing, network and update controls remain functional, not mock values.
- Chapter arrows and a scrollable selection list with a separate Open action. Browsing does not seek until Open is pressed; reversed scrolling is respected.
- Appearance with all three Typography / Reading / Paused tabs, Try, a real RFont4 preview and compact paginated controls. Large neighbours that cannot fit the smaller preview are omitted. Reading/paused visibility, battery/footer formats, font selection, tracking, anchor, guides, highlighting and phantom words still use the existing settings.
- Horizontal reader chrome with Menu, chapter selection, sentence rewind, WPM adjustment, start/pause, battery and progress. Controls stay outside the established reading canvas; word gestures, rendering geometry, shaping, phantom placement and pacing are preserved. Vertical books retain their existing controls. New control captures cancel when dragged away or when leaving the reader.
- Focus preset selection and large digital session timer, phase/round status and an end-session confirmation. The orientation-controlled session and timer lifecycle are unchanged; no background timer has been introduced.
- Proportional, antialiased Atkinson UI text from the repository's OFL font, with code-native navigation arrows and vector icons. Approximately 216 KB of read-only glyph data; no font download is needed. Unsupported scripts and installed locale fonts retain the existing fallback path.
- An Ink presentation for the unmodified stock palette, including the live reader. Installed/custom theme colours are not replaced. The reading canvas does **not** use the menu font.

Runtime job/status screens use the matching branded layout too. Keyboards and the Focus preset editor inherit the new typography and controls while retaining their established field geometry. Permanent encryption warnings and screensavers retain their established layout. Other display sizes keep their existing UI. Some new explanatory copy is English; existing translated labels and locale-pack formats are preserved. This is a close native interpretation, not a pixel-identical browser port.

## Startup and loading

The LCD 3.49 now has a matching first-boot presentation: a compiled-in RSVP Nano wordmark, Ink typography/palette, a quiet divider, bounded status text and a thin progress rail. The regular status callback is given startup presentation context for the duration of `App::begin()`, so SD/library loading and initial book-open messages do not switch back to the old layout halfway through startup. Runtime messages use the same layout with a different heading; other panels retain their existing layout.

Storage, settings, locale, font and library checks display their actual stage. Percentages are displayed only when supplied by an existing loader callback; they describe that operation, not an invented overall boot percentage. Unknown progress has no filled bar or numeric estimate. Error titles and recovery hints continue through the same status callback. The startup presentation flag is cleared after initialization without painting over the last reported message.

There are no new sleeps, minimum animation durations, tasks or changes to initialization order, stored settings, phone APIs or reading behaviour. The existing 650 ms splash deadline is unchanged. Extra status drawing has a real rendering cost, so this is not a claim of identical measured boot time. Compiled-in menu glyphs and vector strokes do not need SD assets to show the first screen. Installed/custom themes are respected once loaded.

Generate the native screenshots with:

```sh
python3 test/native_modern_preview/run.py build/ui-preview-edition
```

Review `startup.png`, `startup-book.png` and `startup-error.png`. Book names and progress in these captures are fixtures rendered by the production status screen. They are not evidence of a physical-device boot.

The startup regression checks cover screen bounds, long text, unknown/out-of-range percentages, unchanged-frame redraw suppression, custom themes, non-target display sizes, and clearing the startup context for runtime status. Also check cold boot on hardware with a working SD card, no SD card, a long saved title, and a failed book load before calling the flow device-verified.

## Compatibility boundary

No changes to the companion HTTP or serial implementation, mobile/web apps, settings schema, public resource models, storage formats, book conversion, USB transfer lifecycle, reading loop, RSVP/page rendering algorithm or phantom shaping/placement code.

The browser's fake phantom-word layout, sample books, pretend battery values, uploads and background Focus behaviour have not been ported. The live reader's focus-letter colour is warm in the stock Ink presentation; custom themes keep their established accent. Book details are read-only; per-book font/language settings apply to the currently open book through Text & display → Language, sleep & book fonts → Typeface.

## Reproduce

```sh
pio test -e native_test -f test_modern_ui -f test_ui -f test_fonts -f test_locales -f test_theme
pio test -e native_test -f test_companion_serial -f test_settings -f test_focus -f test_pacing
pio test -e native_watch_test -f test_watch_ui
python3 tools/test_firmware_targets.py
python3 test/native_modern_preview/run.py build/ui-preview-edition
pio run -e waveshare_esp32s3_touch_lcd_349_rev1
```

The preview runner uses the real Arduino_GFX rasterizer and production screen layouts, with fixture metadata/catalogs/hardware and no device I/O. Appearance and reader words are static RFont4 specimens, not a simulation or test of the live reader's shaping/phantom pipeline. Focus uses the real session model with fixture IMU availability. The runner also checks all four built-in RFont4 sizes against the clipping boundary and exercises production reader-control routing with persistence/playback spies. PNG captures are generated in `build/ui-preview-edition/`, covering the main screens, appearance tabs, Focus, loading, long/empty library cases and detailed settings.

`tools/generate_ui_sans.py` reproducibly generates `src/fonts/UiSans.generated.h` from the checked-in font using Pillow and fontTools. Its source font digest and OFL attribution are embedded in the generated header.

## Preview Edition verification — 22 September 2026

- 194 native tests passed: 14 modern UI, 54 existing UI/reader, 114 fonts/locales/themes/settings/serial/Focus/pacing, and 12 watch-layout tests.
- Four firmware-target/export tests passed. Native preview checks passed for all four RFont4 sizes, reader control routing/bounds/drag cancellation/vertical pass-through, Appearance tabs and independent visibility, and Focus start/end confirmation.
- Native previews inspected for Menu, Library, reader chrome, Settings, Appearance, Focus, loading and deeper pages. These are host renders, not device photographs. Hardware, catalog and persistence/playback fixtures are described above.
- Menu glyph generation reproduced byte-for-byte; `git diff --check` passed. Companion apps/APIs, settings schema, reading loop, Focus engine, USB, library/storage and AlphaFont rasterizer sources have no changes against the baseline.
- Final LCD 3.49 rev1 firmware build succeeded. Reported RAM: 124,516 / 327,680 bytes (38.0%); application flash: 3,204,370 / 6,553,600 bytes (48.9%).
- Application file: 3,204,896 bytes; SHA-256 `d55257c57f69b0bb3b62682f52d67419efed1434160edabb9d927304ee8cef65`.
- Complete installer: 3,270,432 bytes; SHA-256 `f3b3326e077b3b96b887215efb9c2aed10093a028b4747c7814dca727bfbb682`.
- Separate laptop package: `~/Downloads/RSVP-Nano-Preview-Edition-LCD349-Rev1-2026-09-22/`, with the complete image, instructions, checksum, these notes, font notices and native screenshots. Use the project installer with **Choose a firmware file → Install selected file**, erase off. Do not flash a merged image directly at zero if preserving settings.

This build has not been flashed by this change; physical-device and phone smoke tests remain outstanding. The first package's firmware hash was checked and remains unchanged. No remote publish, source commit, or phone-app update was performed.

## Earlier reader-first build (retained for rollback)

- 189 native tests passed across modern UI, existing UI/reader rendering, fonts, locales, themes, settings/API contracts, serial framing, pacing, Focus and watch layouts.
- Four firmware-target/export tests passed. The real-canvas preview also passed its four-strike RFont4 clipping check.
- Generated menu font reproduced byte-for-byte; `git diff --check` passed.
- LCD 3.49 rev1 firmware compiled successfully. Application image: 3,046,384 bytes, at `.pio/build/waveshare_esp32s3_touch_lcd_349_rev1/firmware.bin`.
- Application SHA-256: `142757365e72a3d06b446c8f69f4540adec3899985a0f1f12f28690324a2435f`.

The compiler still reports warnings in bundled PDFio/HarfBuzz/Arduino dependencies; those implementations were not changed. Mobile applications were not rebuilt, and no physical Nano/phone connection was tested. The compatibility evidence is unchanged API code plus passing native contract tests, not an on-device certification.

## Before calling it device-verified

This is a local development build, not a published release. It has not been flashed by this change. Verify the board revision before choosing a binary; the rev1 build is **not** a recommendation to flash it to a rev2 device.

On the actual device, check:

1. Native-size title readability, edge clipping and touch targets, including long titles and left-handed rotation.
2. RSVP and page reading at each font size; phantom words on/off, long words, custom fonts and multilingual books.
3. Appearance changes and per-book overrides after returning to reading and after reboot.
4. Phone connection, list/upload/open a book, settings edits, reconnect and saved reading position. Also check web USB and safe mass-storage eject.
5. Focus flip/pause/round completion and exiting a session; brightness persistence and standby.

Do not infer a physical-device or phone smoke-test result from the native screenshots or host tests.
