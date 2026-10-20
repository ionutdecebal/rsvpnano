# RSVP Nano — New UI preview

This branch is a candidate for **Waveshare ESP32-S3 Touch LCD 3.49 Rev1 only**.
It keeps RSVP Nano branding and the Pocket Lines logo reading guides.
Classic remains the stable/default release. This is a separate firmware choice;
there is no live interface switch on the device.

## Included

- Gesture-led reader, swipe-up speed/brightness controls, larger touch targets.
- Consolidated menus, full-tile sliders, appearance and per-book language/font controls.
- Word and centred chunk playback; page browsing, chapters and text search.
- Background Focus sessions, timer-only use, presets, full-screen breaks and manual resume.
- First-use introduction with physical buttons, speed/pacing practice, SD checks,
  book-upload QR/instructions and a dictionary lesson. Replay under Settings → Help.
- English offline dictionary, typed search and optional direct Wi-Fi lookup.
- Existing web companion, USB book transfer, Wi-Fi, storage/encryption and GitHub updates.

The experimental notes, microphone recording, Bluetooth phone sync, transcription
and new Android app are **not part of this build**. Their existing SD files are
left alone. The complete prototype is maintained separately.

## Install and return to Classic

1. Confirm **LCD 3.49 Rev1**. Other boards should continue using Classic.
2. Save a copy of the SD card, especially settings and reading progress.
3. In the [web companion](https://ionutdecebal.github.io/rsvpnano/), choose your
   board. Choose **New UI (preview)** when listed, or **Choose a firmware file**
   and select the candidate's merged `rsvp-nano-esp32-s3-touch-lcd-3.49.bin`.
4. Leave **erase first off**. Use the companion's segmented installer. A generic
   whole-chip erase or writing a merged image across NVS can remove settings.
5. For an application-only update use `rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin`,
   never the merged image. Existing OTA partition offsets are unchanged.

Returning to Classic is an intentional installation of the Classic image for the
same board. Do not erase or format the SD. Classic may ignore new settings fields
or replace them when it saves; keep the SD backup if you want to restore the
preview's exact preferences. Physical upgrade/rollback compatibility is part of
the acceptance checklist below, not established by a successful build.

If this device has already completed an earlier introduction, it does not repeat
automatically. Use Settings → Help → Replay introduction.

## Install the offline dictionary

Extract `RSVP-Nano-English-Dictionary.zip`. Copy its `dictionaries` directory to
the **root of the SD card**, using a card reader or USB mass-storage mode:

```
/dictionaries/english.oewn
/dictionaries/ATTRIBUTION.txt
/dictionaries/manifest.json
```

The pack contains 131,094 keys including form aliases and occupies 50,137,998
bytes (47.82 MiB). It is an English dictionary; it does not install additional
interface translations. No account, phone or server is needed for offline use.
Firmware alone does not include the pack; a missing-pack message explains setup.

From a paused book, swipe up → Dictionary. Search types a different word; the word
arrows inspect nearby words without moving your saved reading position.
Definition arrows advance through pages and meanings. **Look online** is an
explicit request directly to Wiktionary over configured Wi-Fi. Only the lookup
word is sent; the book is not uploaded. Failure leaves offline lookup available.

The SD pack adapts [Open English Wordnet 2025](https://en-word.net/), by the Open
English Wordnet Community, derived from Princeton WordNet, under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changes include compact
definitions/examples, aliases and a maximum of eight senses per device entry.
Keep the attribution with redistributed packs.

Online text is from Wiktionary contributors under
[CC BY-SA 4.0](https://en.wiktionary.org/wiki/Wiktionary:Copyrights).
The original entry is `https://en.wiktionary.org/wiki/<lookup-word>`; its history
lists contributors. The firmware removes HTML and displays a bounded selection
of English meanings. Data licences do not replace the firmware's existing licence.

## Preview update channel

An empty update tag in this build resolves to
`preview-ui-dictionary-20261020.1`, not GitHub's latest stable release.
Explicit owner/repository/tag settings remain supported. If upgrading from a
custom development build, inspect Connections → Wi-Fi → Release tag before
updating; saved custom values are not silently discarded.

Future preview tags must be selected explicitly until a dedicated preview feed
is introduced. Classic's `latest` stays stable and does not select prereleases.
The old private ecosystem updater is not compiled into this firmware.

## Review and acceptance

Open `RSVP-Nano-New-UI-Review.html` locally in a browser. It is self-contained and
uses production raster captures with synthetic fixtures. Choose a screen, click
an area to pin a comment, set priority, and export feedback as JSON or Markdown.
JSON import keeps comment IDs across regenerated boards. Browser autosave is not
a backup: export before moving the file or changing browsers.

Host tests verify layouts and logic. Before public firmware publication, record
results on the Nano for:

- Fresh introduction, skip/replay, absent SD and absent dictionary.
- Bottom-edge taps/sliders, both orientations, long titles and both themes.
- Book upload/open/search/page return and progress after restart.
- Word/chunk reading at each size, including the largest font at 1000 WPM using
  the [USB benchmark](../usb-reader-benchmark.md); long-book reading and heap behaviour.
- Offline search, definition paging, repeated lookups, online success/no entry,
  absent Wi-Fi and server failure.
- Focus timer-only, reading session, break and manual resume.
- Sleep/wake, settings/encryption, Classic upgrade and deliberate rollback.

This candidate has not been physically verified merely because the build and
native tests pass.

## Maintainer publication

Build the accepted source revision with a unique local `preview-ui-dictionary-*`
tag using `tools/export_web_firmware.py --env waveshare_esp32s3_touch_lcd_349_rev1`.
The New UI candidate workflow can also build one target and upload CI artifacts;
it never publishes automatically or moves remote tags.

Publish the **tested artifacts** as an immutable GitHub prerelease, with both
firmware images, `SHA256SUMS.txt`, dictionary ZIP/attribution, review HTML, source
revision and these instructions. Do not mark it latest or overwrite older assets.

The installer change is separate from the firmware branch. After acceptance and
publication, set the repository Actions variable `RSVP_UI_PREVIEW_TAG` to that
exact published tag and deploy the narrow installer branch. The staging tool
verifies the image checksum and places the preview under a distinct filename.
An unset variable leaves the Classic-only installer intact. Verify downloads and
both choices after deployment.
