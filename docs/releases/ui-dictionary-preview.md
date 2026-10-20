# RSVP Nano — New UI preview

This branch is a candidate for **Waveshare ESP32-S3 Touch LCD 3.49 Rev1 only**.
It keeps RSVP Nano branding and the current reading-guide artwork.
Classic remains the stable/default release. This is a separate firmware choice;
there is no live interface switch on the device.

## Included

- Touch reader, middle-button speed/brightness controls, larger touch targets.
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

Returning to Classic is an intentional installation of the selector-equipped Classic image for the
same board, through Firmware updates or the browser installer. Do not erase or format the SD. Classic may ignore new settings fields
or replace them when it saves; keep the SD backup if you want to restore the
preview's exact preferences. Physical upgrade/rollback compatibility is part of
the acceptance checklist below, not established by a successful build.

If this device has already completed an earlier introduction, it does not repeat
automatically. Use Settings → Help → Replay introduction.

## Install the offline dictionary

During onboarding, or Dictionary → ? → Install / manage, choose **Install English dictionary**.
This downloads about 48 MiB over Wi-Fi to the SD card from the independent
[English dictionary data release](https://github.com/ionutdecebal/rsvpnano/releases/tag/dictionary-en-2025.1).
English is available now; more languages are planned.
An existing pack requires explicit replacement confirmation and remains intact until verification.
The middle button cancels; retry restarts the transfer.

Alternatively, extract `RSVP-Nano-English-Dictionary.zip`. Copy its `dictionaries` directory to
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

From a book, press the middle button → Dictionary. Search types a different word; the word
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

## Feedback pass — 20 October, candidate 2

- The middle button opens quick controls from the book and returns to the paused
  book when pressed again. Swipe-up and the small handle are removed. The visible
  Menu target opens the full menu; the right button still plays/pauses.
- Navigation occupies the left of quick controls, speed and Back to book the
  centre, brightness the right. The introduction teaches the same shortcut.
- Font/size selectors have equal widths. Touch test is in Display, including its
  return path. Upload instructions align their numbers to the first text line.
- The physical-button drawing uses the case/display aspect ratios from the
  [Waveshare mechanical drawing](https://docs.waveshare.com/assets/images/ESP32-S3-Touch-LCD-3.49-ProductSize-1-b798e9adb46e2dde7847e5227739baf6.webp).
  Button order follows the founder's Rev1 enclosure; the redundant orientation
  caption is removed. The page lesson now has measured, flowing text.
- Up to five Wi-Fi networks are remembered in the existing private NVS secrets
  store. Adding a sixth replaces the least recently selected network. The old
  single-network credentials migrate when adding another network. Saved networks
  can be selected without typing; Forget selected removes only that network.
  Dictionary, RSS and OTA scan for saved networks and try the strongest available
  first, with bounded fallback attempts. The local companion chooses an available
  saved network at startup and retains its direct access-point fallback. Wi-Fi
  remains on demand; this does not keep the radio running during normal reading.
- The saved-network list includes out-of-range networks. If a password changes,
  forget that network and add it again. Passwords are not mirrored to SD settings
  or returned through the public settings API.

## Feedback pass — 20 October, candidate 3

- The reading screen has no Menu button. Open quick controls with the middle
  button, then Menu. Text playback and the chapter/progress targets remain.
- Dictionary onboarding has room for both online explanation lines in English,
  German and Romanian; navigation arrows use consistent antialiased strokes.
- The modern page browser uses the smooth Small strike of the selected book font.
  Returning to the same word restores the chosen RSVP size immediately while
  paused. The page and RSVP caches are invalidated on font-role transitions;
  paused browsing no longer runs chunk preparation against the page font.
- Chunk mode hides guides entirely. Its Guides tab explains that they require
  Word mode, and preserves guide size/gap for switching back.
- Hold-to-read release now respects the selected pause mode, matching tap and
  physical-button playback. Requested sentence-end pauses are enforced within each reading-loop catch-up
  step. This prevents delayed frames or work crossing a word deadline from
  skipping the requested stop. Two-letter endings such as “it.” are no longer
  automatically classified as initials. Sentence recognition remains heuristic.

Candidate 3 is delivered as a manual browser-flash BIN. The prototype development
OTA feed still points to candidate 2; it has not been repointed for this pass.

## Feedback pass — 20 October, candidate 4

- Connections now has Wi-Fi, the local companion, USB and RSS. Dictionary stays
  in quick controls; its **?** button opens installation/online help and returns
  to the same lookup.
- Library replaces Book details with Book settings for the selected book.
  Language/font overrides persist without opening that book or changing either
  saved position. Unindexed imports are prepared by the loading worker. Failed
  saves are reported and leave the draft unchanged; override writes use a
  temporary file and replacement backup. Changes to the currently open book
  also refresh the reader's live overrides/font.
- Reading settings no longer duplicates Book settings. Interface language is in
  Device; Display keeps theme, standby and screensaver controls. Introduction
  directions and the review board follow these routes.
- Page browsing continues to use the exact **Small** strike of the book font,
  as confirmed by the founder (not Compact). Candidate 3 already selected that
  strike; no additional scaling or smaller substitute is introduced. Repeated
  page/reader transition tests still check all RSVP/chunk sizes while paused.

Candidate 4 is a manual browser-flash BIN. The development OTA feed and public
firmware releases are unchanged; physical acceptance is still pending.

## Candidate 5 — updates and dictionary installation

- Firmware updates checks the current interface, or explicitly checks the other one.
  The selected version is shown before Confirm installation. New UI offers Return
  to Classic; the narrow Classic maintenance build offers Try New UI on Rev1.
- Empty official update settings follow the corresponding entry in
  `https://ionutdecebal.github.io/rsvpnano/firmware/channels.json`.
  Custom repository/tag settings still work. Switching backs them up in NVS
  `rsvp_updates` (`new-ui-source` / `classic-source`) and clears the active override.
  This recovery copy is not automatically restored or exposed as a source picker.
- HTTPS certificate verification, bounded streaming, exact lengths and SHA-256
  protect OTA delivery. Only the inactive app slot is written; the main task saves
  switch settings before selecting it. This is not automatic post-boot health rollback.
- Classic accepts saved `chunks` mode as single-word RSVP. Native cross-codec tests
  retain speed and Wi-Fi. Interface-specific settings may still be replaced on save.
- Dictionary setup and the introduction offer SD installation with progress, cancel,
  retry, free-space checks, SHA-256 verification of both stream and saved file, and
  staged recovery. The manifest and attribution travel with the pack. Manual
  compatible packs remain supported and require confirmation before replacement.
- The public catalog and raw dictionary assets are not deployed by this candidate.
  Their unavailable state is shown as a retryable error. Manual SD installation works.
- The previous private development OTA feed remains unchanged. Use the provided
  merged image in the existing browser installer for this local candidate.

## Candidate 6 — independent English dictionary download

- English remains the only offered dictionary. Setup, help and onboarding explain
  that more languages are planned; no empty language selector is added.
- The installer uses `dictionary-en-2025.1` data assets independently of firmware
  v0.2.0. Candidate 5 still points to v0.2.0; install candidate 6 to use the new URL.
- The original 50,137,998-byte dictionary pack and its SHA-256 are unchanged.
  Attribution includes the original WordNet notices; the ZIP also includes the
  upstream licence texts. Manual copy and custom pack replacement still work.
- Only dictionary data is published in this pass. Classic remains latest/stable;
  firmware releases and the public OTA catalog still await acceptance.

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
- Sleep/wake, settings/encryption, old Classic → selector Classic → New UI →
  selector Classic → New UI, normal updates and progress preservation.
- Dictionary download with missing/full/read-only SD, lost Wi-Fi, cancellation,
  retry, interrupted replacement, custom-pack preservation and offline lookup.
- Actual public-catalog OTA delivery and dictionary download after staging/publishing
  the immutable assets; the native transport tests use a simulated HTTP backend.

This candidate has not been physically verified merely because the build and
native tests pass.

## Maintainer publication

Build the accepted source revision with a unique local `preview-ui-dictionary-*`
tag (public New UI tag **v0.2.0**) using `tools/export_web_firmware.py --env waveshare_esp32s3_touch_lcd_349_rev1`.
The New UI candidate workflow can also build one target and upload CI artifacts;
it never publishes automatically or moves remote tags.

Publish the **tested artifacts** as an immutable GitHub prerelease, with both
firmware images, `SHA256SUMS.txt`, `update-channel.json`, the dictionary ZIP,
review HTML, source
revision and these instructions. Do not mark it latest or overwrite older assets.

The installer change is separate from the firmware branch. After acceptance and
publication, set the repository Actions variable `RSVP_UI_PREVIEW_TAG` to that
exact published tag and deploy the narrow installer branch. The staging tool
verifies the image checksum and places the preview under a distinct filename.
An unset variable leaves the Classic-only installer intact. Verify downloads and
both choices after deployment.

The selector Classic build needs its own immutable stable 0.1.x maintenance release
and descriptor. The staging script includes only descriptor-equipped Classic
releases as switch targets. Publish that maintenance release before enabling the
New UI catalog entry; older Classic remains available but is not the round-trip target.
New UI uses a build-only workflow; the Classic workflow refuses existing releases,
never force-moves tags, and is restricted to Classic 0.0.x/0.1.x tags. Pages always
builds main, never a firmware release branch. Merge the separate installer and
README changes only when their referenced releases are available.

Dictionary data is hosted separately under `dictionary-en-2025.1`, marked as a
prerelease and explicitly not Latest so firmware discovery is unaffected. Use
`tools/dictionary/package_release.py` to prepare verified raw assets, the manual
ZIP, licences and checksums. Attach all assets before publication; never replace
published packs. A new pack gets a new data version. This release offers English
only, and must not be represented as multi-language support.
