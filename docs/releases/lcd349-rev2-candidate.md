# LCD 3.49 Rev2 — v0.2.0 release record

The founder tested the v0.2.0 UI/dictionary candidate on a confirmed LCD 3.49 Rev2
and reported that it performs well. On 23 October 2026 they approved pushing the
tested source branch and updating the README to show Rev2 support.
The founder subsequently approved publishing the tested firmware, browser installer
and on-device updates ("okay let's do that"). Branch: `work/ui-020-rev2`.
The source-only approval below was extended explicitly, not inferred.

Base: published `v0.2.0`, commit `cf42168db427b4a0e96601d4f093313e5cbfb243`.
Tested firmware commit: `73af8bc7c4fdd4bd3c665a447f2fa2c5b3f8cf92`.
Subsequent README/status changes do not change the tested firmware code.
Release tag: `v0.2.0-rev2.1`. Never move the published v0.2.0 tag
or replace its assets. This retains v0.2.0 features and branding; it is not the
notes/Bluetooth/app prototype or Pocket Lines firmware.

## Hardware changes and evidence

The existing Rev2 environment already selects GPIO42 for backlight, GPIO8 for
the expander interrupt/touch wake and expander pin 5 for display reset. Rev1 uses
GPIO8, GPIO42 and MCU GPIO21 respectively. Both panels are 172 × 640 and use the
same AXS15231B renderer. SD, buttons, touch controller and UI layout remain shared.

Rev2 now receives an explicit high/low/high display reset through the TCA9554,
with 30/250/30 ms delays before panel initialization. Previously only the high
level was written. Failed expander access stops display initialization with a log
instead of claiming success. Rev1 still uses the graphics library's GPIO reset.

Reference: Waveshare's pinned Rev2 [configuration](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/Arduino/examples/09_LVGL_V8_Test/user_config.h)
and [reset sequence](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/Arduino/examples/09_LVGL_V8_Test/lvgl_port.c).

Keep the existing inverted backlight PWM. The Rev2
[schematic](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/schematic/ESP32-S3-Touch-LCD-3.49%20V2.pdf)
shows LCD_BL feeding the boost controller's feedback node through R52, consistent
with the inverted duty values in the vendor PWM helper. A separate vendor LVGL
example uses plain GPIO levels, so it is not a brightness calibration reference.
The existing 40 MHz QSPI configuration, SPI host and minimum brightness mapping
are unchanged. The owner's general hardware test passed; it did not include a
separately reported measurement of the brightness curve or sleep/wake matrix.

## Updates and artifacts

- New UI catalog validation now accepts Rev2 while still requiring exact board,
  OTA filename, checksum, size and version. It never substitutes a Rev1 entry.
- Descriptor generation accepts `--board lcd349-v2` and checks the board's OTA
  asset identity embedded in the binary. Rev1 remains the default.
- The build-only candidate workflow has an explicit revision selector. It does
  not publish anything, and has not been dispatched for this candidate.
- Flash layout and all partitions remain unchanged. Browser merged images and
  OTA application images keep their existing distinct revision-specific names.

The public delivery uses four board/channel entries: Classic v0.1.5 and New UI
for each LCD revision. Rev1 New UI remains v0.2.0; Rev2 uses v0.2.0-rev2.1.
**Check current software** keeps the interface and **Return to Classic** selects
the Rev2 Classic image. Classic v0.1.5 exposes **Try New UI** on both revisions.
Older Classic installations first update normally to v0.1.5. A physical Rev2 OTA
round trip remains unreported; host checks establish artifact/catalog compatibility.

Installer/catalog changes are separate from this firmware branch and based on
current main. They preserve Classic as default, independent board versions, both
Rev1 entries, legacy OTA aliases and the bounded release listing. The complete
publication record is in `docs/releases/rev2-delivery.md` on main.

## Build and local verification

For a normal build of this source branch, use the Rev2 instructions in the README.
To reproduce the exact tested binary with the commands below, first check out
tag `v0.2.0-rev2.1`, which points to
`73af8bc7c4fdd4bd3c665a447f2fa2c5b3f8cf92`. Do not recreate or move this tag.

```sh
python3 tools/test_update_descriptor.py
python3 test/native_update_delivery/run.py
python3 test/native_update_delivery/roundtrip.py /path/to/classic-checkout
RSVP_FIRMWARE_TAG=v0.2.0-rev2.1 python3 tools/export_web_firmware.py --env waveshare_esp32s3_touch_lcd_349_rev2
python3 tools/make_update_descriptor.py --board lcd349-v2 --channel new-ui \
  --image build/firmware/rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin \
  --tag v0.2.0-rev2.1 --commit "$(git rev-parse HEAD)" \
  --output build/firmware/update-channel.json
```

Before tagging, commit only the reviewed local source so the binary is cleanly
versioned. Verify the merged image embeds the exact application, and compare its
partition bytes to the published Classic v0.1.4 Rev2 image. Preserve ELF/map and
checksums locally. Also compile Rev1 when changing the shared display backend.
Host checks cannot establish hardware performance or physical OTA compatibility.

## Device acceptance

The owner's general Rev2 test passed on 23 October 2026. The checklist below
records the intended acceptance coverage; it is not evidence that each item was
individually tested. In particular, no Rev2 1000 WPM benchmark or on-device OTA
switching result has been reported.

Install the candidate **merged** Rev2 BIN with Choose a firmware file in the
browser companion, selecting LCD 3.49 Rev2. Keep erase first off. Back up SD
settings/progress first; never format the card. Use the companion's segmented
installer so NVS is preserved. Do not flash this file to Rev1 or an AMOLED board.

1. Cold start and several reset-button/software restarts: correct screen and
   orientation every time; no black screen, garbage or restart loop.
2. Replay onboarding in Help. Confirm physical button diagram/order, SD check,
   dictionary explanation and all bottom navigation targets.
3. Test touch at all edges, WPM and appearance sliders, in both orientations.
4. Sweep brightness from 1% to 100% and back, then test idle sleep and waking
   with the middle button and touch. Repeat while on battery and on USB.
5. Open an existing book, browse/search pages, return paused, switch word/chunk
   modes and fonts/sizes. Confirm saved position survives a restart. Check guide
   visibility, sentence-end pause and normal reading at your usual speed.
6. Run the existing 1000 WPM benchmark if claiming that speed on Rev2; a build
   or visual impression alone does not establish timing or draw performance.
7. Offline English lookup, a missing word, explicit online lookup, and dictionary
   install/cancel/retry. The same independently released English pack is used.
8. Focus alongside reading and standalone, full-screen break, manual resume.
9. Wi-Fi connection, USB mass-storage and book import. Confirm no SD loss.
10. Firmware updates must offer the matching Rev2 image, never Rev1. Switch
    to Classic v0.1.5 and back to New UI; verify books, progress and Wi-Fi remain.

Report any failures with the installed version and board revision.
