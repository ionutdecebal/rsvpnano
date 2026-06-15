# ESP32-S3 Multi-Target Layout

## Goal

Keep the app code board-independent while supporting multiple ESP32-S3 Waveshare boards from one
source tree. PlatformIO selects one physical board at build time; shared code should not need
`#ifdef` branches for display chips, PMUs, expanders, touch controllers, or board button wiring.

## Current Structure

`src/board` is the stable app-facing API. It declares what the app may ask the board to do:

- `BoardDisplay.h`: display init, sleep, wake, brightness, and backlight capability.
- `BoardInput.h`: raw logical controls, raw touch contacts, and timing facts used by `src/input`.
- `BoardPower.h`: battery status, wake, sleep, shutdown, and power-button support.
- `BoardStorage.h`: SD card bus setup and frequency probing.
- `BoardImu.h`: orientation sensor operations used by Focus Timer.
- `BoardAudio.h`: app beep/audio operations.

`src/platforms/<board>` contains the implementation for one board family. These files choose the
real buses, pins, rails, reset order, and selected chip drivers for that board. Board-owned
`Waveshare*.h` headers hold private `constexpr` wiring and timing facts, split into small namespaces
such as display, touch, power, input, storage, and IMU.

`src/drivers` contains reusable chip code. Drivers validate their own chip-level facts, such as I2C
addresses, packet sizes, register masks, and transfer limits. They should not know about public
`Board::Config` facts or app states.

`src/input/Input.h` and `src/input/Input.cpp` own input-domain behavior: debouncing, press duration,
triple press detection, touch movement tracking, edge gestures, and the final logical events consumed
by `App`.

## Board Config

`Board::Config` is intentionally small. It is for public board facts that app/UI code legitimately
needs, such as board identity, asset names, display dimensions, default orientation, UI margins, and
safe public limits.

Driver-specific wiring does not belong in `Board::Config`. For example, a TCA9554 address, AXP2101
IRQ pin, backlight GPIO, touch controller packet size, or audio bus choice belongs in that board's
private platform header and the matching `Board*.cpp` implementation.

## Input Flow

The app does not check physical buttons directly. The flow is:

```text
physical GPIO, PMU, expander, or touch controller
  -> platform Board::Input raw reads
  -> Input::poll debounce and gesture tracking
  -> Input::Event with logical controls
  -> App state-specific handling
```

The logical controls are `InputPrimary`, `InputPower`, `InputKey`, and `InputTouch`. A board maps its
physical controls to those names in its platform implementation. The app then decides what a short
press, long press, triple press, tap, swipe, or touch event means in the current app state.

## PlatformIO Selection

The base PlatformIO configuration excludes all platform and driver folders. Each environment includes
one platform folder and only the driver folders used by that board. This keeps unused implementations
out of the build instead of relying on runtime checks.

Current firmware environments:

- `waveshare_esp32s3`
- `waveshare_esp32s3_rev2`
- `waveshare_esp32s3_usb_msc`
- `waveshare_esp32s3_usb_msc_rev2`
- `waveshare_esp32s3_touch_amoled_18`
- `waveshare_esp32s3_touch_amoled_216`
- `waveshare_esp32s3_touch_amoled_241`

The benchmark environments extend these firmware environments and add `RSVP_BENCHMARK_MODE`.

## LCD 3.49 Revisions

The LCD 3.49 rev1 and rev2 boards share one platform folder:

```text
src/platforms/waveshare_lcd_349/
```

Common facts live in `WaveshareLcd349.h`. The small hardware differences live in:

```text
src/platforms/waveshare_lcd_349/rev1/WaveshareLcd349Revision.h
src/platforms/waveshare_lcd_349/rev2/WaveshareLcd349Revision.h
```

PlatformIO selects the revision header with `RSVP_BOARD_REV_HEADER`.

## Adding A Board

1. Add a new `src/platforms/<board>` folder.
2. Add a private board header with the physical wiring and timing facts.
3. Implement the required `Board*.cpp` files by binding those facts to the shared board API.
4. Reuse drivers from `src/drivers` where possible.
5. Add a PlatformIO environment that includes only that platform folder and its selected drivers.
6. Keep app behavior in `App`, `DisplayManager`, `Input`, and domain modules, not in the platform folder.

The desired boundary is simple: adding a board should mostly mean adding a platform folder and build
environment, not teaching app/storage/display logic about another hardware variant.
