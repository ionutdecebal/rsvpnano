# ESP32-S3 Touch AMOLED 2.41 Bring-Up Notes

## Purpose

This note captures the current state of the Waveshare `ESP32-S3-Touch-AMOLED-2.41` port so we can
return to it safely later, or repeat the process on similar boards without rediscovering the same
hardware quirks.

## Checkpoint Context

- Original bring-up started on local `main` at `v0.0.3`
- Port checkpoint branch: `checkpoint/esp32s3-amoled-241-v003-base`
- Current integrated branch: `integration/esp32s3-amoled-241-v005`
- Current repo base: upstream `v0.0.5`
- Current working port target: `waveshare_esp32s3_touch_amoled_241`
- Original preserved target: `waveshare_esp32s3`

## Current Hardware Status

What is working on the `2.41` board now:

- Display initializes and renders correctly
- Touch input works and follows display orientation
- Colors are correct
- Layout aligns with the visible panel area
- Fixed on-screen stripe artifacts are gone
- Battery hold and battery ADC path are wired for this board
- SD card loading works on the `2.41`, including with the original device's SD card
- Shared dual-target repo structure is in place
- Deep-sleep power path now keeps the battery-hold GPIO latched instead of dropping board power

What is not yet considered safe or complete:

- OTA is not board-aware yet
- Longer burn-in testing is still needed
- USB MSC transfer is enabled, but still needs hardware burn-in on this target

## Key Architectural Changes

- Added multi-target board selection through `BoardConfig`
- Added board profiles under `src/board/profiles/`
- Preserved the existing `3.49` board as its own target
- Added a new display abstraction layer in `src/display/BoardDisplay.*`
- Added RM690B0 display support in `src/display/rm690b0.*`
- Added FT6336 touch support in `src/input/TouchHandler.*`
- Added board-specific display transfer chunk sizing

## 2.41 Board Assumptions

Current `2.41` profile values live in
`src/board/profiles/WaveshareEsp32S3TouchAmoled241Profile.h`.

Important assumptions:

- Panel native geometry is `450x600`
- App display geometry is `600x450`
- Touch controller is `FT6336`
- Display controller is `RM690B0`
- I2C is shared on `GPIO47/48`
- Touch reset is `GPIO3`
- Battery hold is direct GPIO on `GPIO16`
- Battery ADC is `GPIO17`
- SDMMC pins are `GPIO4/5/6`

## Bring-Up Problems We Hit

### 1. Rotation confusion

Waveshare demo orientation assumptions did not match the observed hardware behavior exactly.

What finally worked:

- Keep panel memory in native portrait-style geometry
- Let the shared app mapping layer perform the quarter-turn into landscape
- Keep touch mapping aligned to the app orientation instead of chasing panel rotation in two places

Lesson:

- Do not try to rotate in both the panel driver and the shared display mapper at the same time

### 2. Incorrect colors

Symptom:

- Red anchor letter appeared blue
- Gray UI elements appeared greenish

Cause:

- The app already byte-swaps RGB565 before sending pixels to the panel
- An additional swap in the RM690B0 driver caused double-swapping

Fix:

- Remove the extra swap from the RM690B0 path

Lesson:

- Always trace the full pixel pipeline before touching display color format code

### 3. Cropped layout and footer drift

Symptom:

- Top `100%` label clipped by the edge
- Footer elements appeared slightly too high

Fix:

- Add a `16px` RM690B0 column offset in `setColumnWindow()`

Lesson:

- If the entire rendered scene is shifted, fix panel addressing first rather than rewriting UI
  layout math

### 4. Fixed black stripe artifacts

Symptom:

- Thin black stripes stayed in fixed screen positions while words moved underneath

Most important finding:

- This was not primarily a color or content-rendering problem
- The stripes were caused by visible seams between display flush bands on the rotated panel path

Fix that solved it:

- Make display transfer chunk size board-specific
- Increase the `2.41` target transfer buffer from `16KB` to `48KB`

Where:

- `src/board/profiles/WaveshareEsp32S3TouchAmoled241Profile.h`
- `src/display/DisplayManager.cpp`

Lesson:

- On rotated panels, native-row chunk boundaries can appear as fixed vertical bands
- If artifacts stay fixed while content scrolls, investigate transport chunking before deeper UI
  logic

## OTA Separation

OTA defaults are now split by board so the `3.49` and `2.41` do not pull the same release asset.

Current behavior:

- The `3.49` keeps using the legacy OTA asset name `rsvp-nano-ota.bin`
- The `2.41` uses its own OTA asset name
  `rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin`
- The release workflow publishes both OTA binaries from fixed build environments
- The updater now rejects obvious cross-board overrides such as pointing a `2.41` at the legacy
  `3.49` OTA asset name

Current limitation:

- The updater still trusts the asset naming convention rather than verifying embedded board metadata
- A deliberately custom `asset_name` that does not match the known board patterns could still bypass
  this guard

## Recommended Next Steps

1. Run broader hardware QA on the `2.41`:
   - sleep and wake on battery
   - SD load behavior
   - battery reporting
   - long reading session stability
   - brightness and dark-mode behavior
2. Hardware-test USB transfer mode for any `2.41`-specific handling.
3. Consider adding board metadata verification to OTA as a second safety layer.

## Build Targets

- `waveshare_esp32s3`
- `waveshare_esp32s3_rev2`
- `waveshare_esp32s3_touch_amoled_241`

## Practical Advice For Future Ports

- Split board specifics before touching rendering logic
- Keep one shared app codebase and compile per-board targets
- Treat display geometry, panel memory geometry, and UI orientation as separate concerns
- Trace the pixel format path before touching RGB/BGR or byte-swap logic
- If artifacts are fixed on the glass, suspect transport or addressing before app layout
- Protect the old board path with separate build targets from day one
