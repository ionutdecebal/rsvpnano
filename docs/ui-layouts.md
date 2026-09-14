# Device UI layouts

Each board's `build_src_filter` selects exactly one presentation. Watch UI is for
watch-like devices, not every AMOLED panel or every board below a diagonal-size
threshold. The following audit uses physical display size and the device's form
factor as well as pixel resolution.

| Device | Diagonal | Native panel pixels | Presentation | Form factor / reason |
| --- | --- | --- | --- | --- |
| LCD 3.49 rev1 | 3.49 in | 172x640 | Regular | Long rectangular reader; active area 22.58x84 mm |
| LCD 3.49 rev2 | 3.49 in | 172x640 | Regular | Same panel and reader form factor |
| AMOLED 1.8 V1 | 1.8 in | 368x448 | Watch | Watch-size panel; active area 28.70x34.94 mm |
| AMOLED 1.8 V2 | 1.8 in | 368x448 | Watch | Same watch-size form factor |
| AMOLED 2.06 | 2.06 in | 410x502 | Watch | Vendor's watch-style enclosure with detachable strap |
| AMOLED 2.16 | 2.16 in | 480x480 | Regular | Square desktop module; active area 38.99x38.99 mm, case 46x46x22.5 mm |
| AMOLED 2.41 V1 | 2.41 in | 450x600 | Regular | Larger rectangular handheld panel; active area 37.22x49.65 mm |
| AMOLED 2.41 V2 | 2.41 in | 450x600 | Regular | Same panel; reset and interrupt wiring differ, not layout |
| ESP32-C6 LCD 1.47 | 1.47 in | 172x320 | Watch | Narrow watch-size display; active area 17.75x32.93 mm |

The 2.16 and both 2.41 revisions use `src/ui/screens/regular/`. The 1.8, 2.06,
and C6 1.47 use `src/ui/screens/watch/`. LCD 3.49 remains regular.

Vendor references: [3.49 LCD parameters](https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49),
[1.8 documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8),
[2.06 watch enclosure](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.06),
[2.16 documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16) and
[dimension drawing](https://www.waveshare.com/img/devkit/ESP32-S3-Touch-AMOLED-2.16/ESP32-S3-Touch-AMOLED-2.16-details-size.jpg),
[2.41 revision table](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.41#v1-v2-differences),
[C6 1.47 documentation](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47).
Physical active-area dimensions are from each vendor page's product drawing.

There is no UI-kind define or runtime boolean. The regular source filter excludes
`watch/`; the watch source filter excludes `regular/`. Both implement the same
screen declarations. Do not include one presentation's implementation from another.

## Ownership

- `src/ui/`: shared rendering, touch handling, cards, rings, rotary controls,
  pagination, keyboard, typography, and theme primitives.
- `src/ui/screens/regular/` and `watch/`: screen arrangement and presentation.
- `src/ui/screens/ReaderLayout.h`: the selected reader presentation's geometry,
  page-font strike, and chrome contract. The shared reading engine calls it without
  inspecting the device or UI kind.
- Shared screen workflow files retain book metadata, network operations, timer
  persistence, and reading state. Existing settings and focus domain types remain
  the source of truth; layouts do not maintain alternative settings or timer lists.

The watch presentation derives bounds from the actual display dimensions. At
320x172, settings and Device actions page through large rows/pairs. USB transfer
and Companion sync occupy the first Device page. Larger displays show more items
at once. Paging does not create hit targets for hidden items.

Chapter and timer carousels only draw three visible cards. Tap a side to select it;
tap the center to activate it. Horizontal swipes select without activating. The WPM
ring uses relative horizontal drag and retains explicit minus/plus controls.

## Orientation is independent of presentation

Selecting watch or regular sources does not change a board's orientation. Keep the
existing board-specific default orientation, native panel addressing, offsets, and
matching touch transform. The 2.41 remains a 600x450 landscape UI over its native
450x600 panel; the 2.16 remains 480x480. Do not assume watch implies portrait.

The existing handedness option uses a 180-degree flip with matching touch mapping.
Panel offsets belong in the display driver, not in layout selection. No new
framebuffer or display-driver replacement is needed to select a presentation.

## Validation

```sh
python -m unittest discover -s tools -p 'test_firmware_targets.py'
uvx platformio test -e native_test -f test_ui
uvx platformio test -e native_watch_test
uvx platformio test -e native_amoled_241_v1_test -e native_amoled_241_v2_test
uvx platformio run -e waveshare_esp32s3_touch_amoled_216
uvx platformio run -e waveshare_esp32s3_touch_amoled_241
uvx platformio run -e waveshare_esp32s3_touch_amoled_241_v2
```

The target regression test covers every production environment and its benchmark
variant, verifies that only the chosen presentation is selected, and checks that
both installer and OTA exports cover the full CI matrix. The reusable firmware
build workflow obtains that matrix from `tools/export_web_firmware.py --list-envs`;
there is no second hard-coded device list to update there.

The watch host tests cover native portrait bounds and the previous landscape sizes;
paging, dock navigation, relative rotary adjustment, and directional carousel
gestures. Host drawing checks do not prove pixel appearance or touch quality on
hardware.

Before release, check on the physical devices:

- The full screen is visible, text is not mirrored, all four corners and dock items
  respond at their drawn positions, and swipes follow the finger in both handedness
  settings. Cold boot and sleep/wake must restore the display.
- Long/localized titles, settings labels, and chapter names remain readable.
- All setting pages, timer creation/editing, and keyboard input remain accessible.
- Side taps and swipes never start a chapter/timer; center taps do.
- RSVP and page modes, vertical CJK, both handedness settings, battery controls,
  and reading-progress taps retain their behavior.
- Timer orientation/pause/complete behavior, USB/Companion operations, theme
  changes, and repeated navigation leave no stale pixels.
- The 3.49 presentation retains its existing appearance and navigation.

No hardware flashing is part of these host/build checks.
