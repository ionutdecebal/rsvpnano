# ESP32-S3 Multi-Target Layout

## Goal

Keep the existing `ESP32-S3-Touch-LCD-3.49` firmware stable while making room for additional
ESP32-S3 boards, starting with `ESP32-S3-Touch-AMOLED-2.41`.

## Key idea

The repo stays single-source, but the hardware target becomes explicit at build time.

- Shared code stays shared:
  - `src/app/`
  - `src/reader/`
  - most of `src/storage/`
  - most of `src/display/DisplayManager.cpp`
- Board-specific details move behind target-specific files and constants:
  - pins
  - screen geometry
  - display driver
  - touch controller
  - power-management backend

## New structure

- `src/board/BoardConfig.h`
  - public board API used by the app
  - selects the active board profile via build flags
- `src/board/profiles/`
  - `WaveshareEsp32S3TouchLcd349Profile.h`
  - `WaveshareEsp32S3TouchAmoled241Profile.h`
- `src/display/BoardDisplay.h`
  - generic display boundary used by `DisplayManager`
- `src/display/BoardDisplay.cpp`
  - maps the active target to the concrete display driver implementation

## Why screen size changes stay isolated

The renderer and touch mapping already read dimensions through `BoardConfig`, for example:

- `BoardConfig::DISPLAY_WIDTH`
- `BoardConfig::DISPLAY_HEIGHT`
- `BoardConfig::PANEL_NATIVE_WIDTH`
- `BoardConfig::PANEL_NATIVE_HEIGHT`

That means the `2.41` target can define:

- `600 x 450`

while the existing `3.49` target keeps:

- `640 x 172` logical
- `172 x 640` native panel

As long as we do not replace the shared constants directly in the old profile, changing the new
target does not alter the old one.

## Current status

The scaffold is in place, but the `2.41` target is intentionally not implemented yet.

The new env in `platformio.ini` exists to mark the target boundary:

- `waveshare_esp32s3_touch_amoled_241`

If someone tries to build it now, the code should stop with explicit compile-time errors in the
unimplemented backends:

- `src/board/BoardConfig.cpp`
- `src/display/BoardDisplay.cpp`
- `src/input/TouchHandler.cpp`

That is deliberate. It protects the current board from half-ported code and makes the missing work
obvious.

## What gets implemented next for 2.41

1. Replace the display backend with an `RM690B0` implementation.
2. Replace the touch backend with an `FT6336` implementation.
3. Confirm the `2.41` SD wiring mode and pins.
4. Implement the `2.41` power/battery behavior.
5. Tune the shared UI layout for the taller, more square display.
