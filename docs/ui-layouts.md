# Device UI layouts

Each board's `build_src_filter` selects exactly one presentation:

| Boards | Screen implementation |
| --- | --- |
| LCD 3.49 rev1/rev2 | `src/ui/screens/regular/` |
| AMOLED 2.41, 2.16, 2.06, 1.8 v1/v2; LCD 1.47 | `src/ui/screens/watch/` |

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

## Validation

Run the two test profiles separately:

```sh
uvx platformio test -e native_test -f test_ui
uvx platformio test -e native_watch_test
uvx platformio run -e waveshare_esp32s3_touch_lcd_349_rev1
uvx platformio run -e waveshare_esp32s3_touch_amoled_206
uvx platformio run -e waveshare_esp32c6_touch_lcd_147
```

The watch host tests cover physical bounds at 600x450, 480x480, 502x410, 448x368,
and 320x172; paging, dock navigation, relative rotary adjustment, and directional
carousel gestures. Host drawing checks do not prove pixel appearance or touch
quality on hardware.

Before release, check on the physical devices:

- Long/localized titles, settings labels, and chapter names remain readable.
- All setting pages, timer creation/editing, and keyboard input remain accessible.
- Side taps and swipes never start a chapter/timer; center taps do.
- RSVP and page modes, vertical CJK, both handedness settings, battery controls,
  and reading-progress taps retain their behavior.
- Timer orientation/pause/complete behavior, USB/Companion operations, theme
  changes, and repeated navigation leave no stale pixels.
- The 3.49 presentation retains its existing appearance and navigation.

No hardware flashing is part of these host/build checks.
