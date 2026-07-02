# RSVP Nano — LilyGo T-LoRa-Pager variant

This document describes the **T-LoRa-Pager** build variant of RSVP Nano and how
to compile and flash it. The default firmware targets the Waveshare ESP32-S3
3.49" board; this variant retargets the same application to the LilyGo
T-LoRa-Pager using the vendor **LilyGoLib** BSP.

> **Status — read this first.** The variant was implemented against the LilyGoLib
> source and the T-Pager hardware docs, but it has **not yet been compiled or
> flashed on hardware from this workspace** (no PlatformIO/ESP32 toolchain or
> device was available here). Treat it as a complete, reviewable foundation that
> needs an on-device build/flash iteration pass. The sections below list the
> exact prerequisites and the one external step (lvgl exclusion) that the build
> needs.

## What changed vs. the Waveshare build

The board layer is a per-platform implementation of the shared `Board::` API.
The T-Pager lives in `src/platforms/lilygo_tlora_pager/` and is selected by the
`t_lora_pager` PlatformIO environment, which compiles that directory's sources
(`build_src_filter`), points `RSVP_BOARD_CONFIG_HEADER` at its `BoardConfig.h`,
and sets the `RSVP_BOARD_TPAGER` flag. The Waveshare builds are untouched.

| Concern            | Waveshare (default)                     | T-Pager (`platforms/lilygo_tlora_pager/`)                      |
| ------------------ | --------------------------------------- | -------------------------------------------------------------- |
| Display            | AXS15231B QSPI, 640×172 (`drivers/display/axs15231b`) | ST7796 SPI, 480×222 via LilyGoLib (`BoardDisplay.cpp`) |
| Orientation        | native-portrait rotated to landscape    | identity (`Portrait`/`PortraitFlipped`) — panel is native landscape |
| Input              | capacitive touch + 2 buttons            | rotary encoder + center button → synthetic touch (`InputTouch.cpp`, replaces the generic `input/InputTouch.cpp`) |
| Board/power        | TCA9554, ADC battery (`platforms/waveshare_lcd_349`) | XL9555 + BQ25896/BQ27220 via LilyGoLib (`BoardPower.cpp`) |
| SD card            | SDMMC (1-bit)                           | shared SPI bus, mounted by LilyGoLib (`installSD()`, see `SdDiagnostics.cpp`) |
| Audio              | direct ES8311 over I2S (`drivers/audio/es8311`) | ES8311 via LilyGoLib codec (`BoardAudio.cpp`)          |
| USB file transfer  | TinyUSB MSC                             | TinyUSB MSC over native USB, SD card exposed at the FatFS block level (`RSVP_USB_TRANSFER_ENABLED=1`, USB-OTG mode) |

All LilyGoLib access is funneled through `platforms/lilygo_tlora_pager/TPagerHardware.h`
(one idempotent `ensureBegun()`). The card filesystem is abstracted with an
`RSVP_CARD` alias (`src/storage/fs/CardFs.h`: `SD_MMC` vs the SPI `SD` object) so
the shared storage code (`StorageManager`, the `converter/` + `storage/`
modules, `RssFeedManager`, `CompanionSyncManager`, `OtaUpdater`, …) is reused;
only the mount path in `SdDiagnostics.cpp` is board-specific.

## Controls

| Action                  | Input                                                    |
| ----------------------- | -------------------------------------------------------- |
| Move menu selection     | Rotate the encoder                                       |
| Select / confirm        | Press the encoder (center button) — haptic confirmation  |
| Start/stop reading      | Double-press the encoder (reader double-tap toggles play) |
| Adjust WPM (while paused)| Rotate the encoder                                       |
| Open / close menu (back)| Short-press **BOOT**                                      |
| Power off (deep sleep)  | Long-press **BOOT** (wakes on **BOOT**)                  |

The PWR button is hardware-only on this device and cannot be read by firmware,
so **BOOT** (GPIO0) is the primary control button and the encoder drives
navigation. Text entry (e.g. Wi-Fi password) uses the physical **TCA8418
keyboard**: on a text-entry screen there are no on-screen keys — type on the
hardware keyboard (Shift and the Space-Fn symbol layer are resolved by the BSP),
**Enter** confirms/saves, **backspace** deletes, and **BOOT** cancels the field.
Masked fields (e.g. Wi-Fi password) are shown in clear text as you type, since
there is no on-screen show/hide toggle on the physical-keyboard path.

## Build prerequisites

1. **PlatformIO** (CLI or IDE).
2. **arduino-esp32 3.x toolchain.** LilyGoLib requires core ≥ 3.3.0-alpha1
   (IDF 5). Mainline `platformio/espressif32` only ships arduino 2.0.x, so the
   `t_lora_pager` environment pins the community **pioarduino** platform fork.
   The version in `platformio.ini`
   (`pioarduino/platform-espressif32@53.03.13`) is a known arduino-3.x release;
   bump it if a newer one is needed. This also provides the
   `lilygo_tlora_pager` board variant (`pins_arduino.h`, `ARDUINO_T_LORA_PAGER`,
   `DISP_*`/`ROTARY_*`/`SD_CS` pin macros) that LilyGoLib relies on.
3. **LilyGoLib + third-party libraries**, checked out as siblings of this repo:

   ```
   <parent>/
     rsvpnano/                ← this repo
     LilyGoLib-master/        ← https://github.com/Xinyuan-LilyGO/LilyGoLib
     LilyGoLib-ThirdParty/    ← https://github.com/Xinyuan-LilyGO/LilyGoLib-ThirdParty
   ```

   `platformio.ini` already points `lib_extra_dirs` at `..` and
   `../LilyGoLib-ThirdParty`, so PlatformIO resolves `LilyGoLib`, `RadioLib`,
   `XPowersLib`, `SensorLib`, the TCA8418 keyboard driver, etc. from these local
   folders.

### Handle lvgl (the one required external step)

This firmware renders its own UI and never uses lvgl, but LilyGoLib bundles
`LV_Helper.cpp` / `LV_Helper_v9.cpp`, which `#include "lvgl.h"`. The env sets
`lib_ignore = lvgl`, so you must also stop LilyGoLib from compiling those two
files. Add a `build.srcFilter` to **`../LilyGoLib-master/library.json`**:

```jsonc
{
  // ...existing keys...
  "build": {
    "srcFilter": "+<*> -<LV_Helper.cpp> -<LV_Helper_v9.cpp> -<USB_MSC.cpp>"
  }
}
```

(`USB_MSC.cpp` is excluded because this variant drives USB mass storage through
its own `UsbMassStorageManager` over the SD card, not LilyGoLib's flash-backed
MSC.) Alternatively, vendor a matching `lv_conf.h` and drop `lib_ignore = lvgl`
— but excluding the unused helpers is simpler.

Note: `LilyGo_LoRa_Pager::begin()` still *references* `setupMSC()` (defined in the
excluded `USB_MSC.cpp`), so excluding that file alone produces an
`undefined reference to setupMSC(...)` at link time.
`platforms/lilygo_tlora_pager/TPagerHardware.cpp` provides a no-op `setupMSC`
stub to satisfy the linker without pulling in TinyUSB.

### Keyboard: suppress the trailing space after "Space + number"

The TCA8418 keymap uses **Space as the symbol/Fn modifier** (the pager has no
dedicated symbol key, so `has_symbol_key == false`). LilyGoLib's
`LilyGoKeyboard.cpp` emitted a space on *every* Space-key release, so typing a
number or symbol via `Space + <key>` inserted the character **and** a stray
trailing space. This repo patches `../LilyGoLib-master/src/LilyGoKeyboard.cpp`
to track a `symbol_modifier_consumed` flag: any key pressed while Space is held
marks Space as a modifier, and its release then emits nothing; a standalone
Space tap still produces a space. Re-apply this patch if you re-clone LilyGoLib
(it lives in `handleSpecialKeys`, `update`, `begin`, and a file-scope static).

## Build & flash

> **Use a dedicated PlatformIO core dir.** This env must build against the
> pioarduino Arduino-3.x core, but the default `~/.platformio` core also holds
> the mainline Arduino-2.0.x framework pulled in by the Waveshare env. Those two
> collide: pioarduino's builder can't resolve `framework-arduinoespressif32` and
> the build dies with `TypeError: expected str, bytes or os.PathLike object, not
> NoneType` (`FRAMEWORK_DIR` is `None`). Keep this variant in its own core via
> `PLATFORMIO_CORE_DIR`. The hardcoded `-I` include paths in the env's
> `build_flags` already assume `D:/pio-tpager`, so use that path (or set it to
> your own and update those flags to match).

```powershell
# PowerShell (set once per shell session)
$env:PLATFORMIO_CORE_DIR = "D:\pio-tpager"
pio run -e t_lora_pager                 # compile
pio run -e t_lora_pager -t upload       # flash over USB-C
pio device monitor -b 115200            # serial (USB CDC)
```

```bash
# bash/zsh
export PLATFORMIO_CORE_DIR=/path/to/pio-tpager
pio run -e t_lora_pager
pio run -e t_lora_pager -t upload
pio device monitor -b 115200
```

Do **not** add `core_dir` to the `[platformio]` section of `platformio.ini` to
achieve this — that is project-global and would force the default Waveshare env
to redownload its entire toolchain into the same core.

If the panel image is upside-down for your grip, switch to the left-handed UI
mode in Settings (a true 180° flip) or set `-DRSVP_TPAGER_ROTATION=2` in the
env's `build_flags`. (Landscape is rotation 0 or 2; rotations 1 and 3 are the
panel's 222×480 portrait orientations.)

## SD card layout

Same as the default firmware — FAT32, with:

```
/books/books        (books)
/books/articles     (articles)
/config
```

## Known limitations / next iterations

The board builds, flashes and runs on hardware. The display, rotary/button
navigation, the legacy flat menu, the button-driven focus timer and the
battery gauge are all verified on-device. The remaining open items:

1. **USB file transfer — needs on-device verification.** Now enabled
   (`RSVP_USB_TRANSFER_ENABLED=1`, USB-OTG mode). The app's
   `UsbMassStorageManager` exposes the SD card over native USB at the FatFS
   block level, so the SPI SD mounted by LilyGoLib's `installSD()` works
   unchanged. This is the one feature that has **not** yet been exercised on
   hardware — confirm the card enumerates on a host and that Serial-over-USB
   (CDC) still works after switching to `ARDUINO_USB_MODE=0`. LilyGoLib's own
   `USB_MSC.cpp` stays excluded (it would expose internal flash, not the SD);
   the `setupMSC` stub in `TPagerHardware.cpp` keeps the linker happy.
2. **Display rotation/mirroring.** `RSVP_TPAGER_ROTATION` defaults to landscape
   rotation 0 (LilyGoLib maps rotations 0/2 -> 480×222 landscape, 1/3 -> 222×480
   portrait). Verified `width()==480, height()==222` on-device.

Accepted as good enough (no further work planned):

- **Text entry** uses the physical TCA8418 keyboard (`Board::Keyboard` →
  `tpager::hw().kb`), not rotary input. Wired and working.
- **Input latency.** `getRotary()` blocks up to 50 ms when idle, so the idle
  loop runs ~20 Hz. Fine for reading at the WPM range this device is used at.
- **Audio cue** plays a 1 kHz tone through LilyGoLib's `EspCodec`; the cue is
  audible and the parameters are accepted as-is.

Resolved:

- **Power consumption.** `LilyGoLib::begin()` still powers up LoRa/GPS/NFC, but
  `TPagerHardware::ensureBegun()` now immediately puts the SX1262 into sleep
  (`radio.sleep()`) and cuts the GPS and NFC rails
  (`powerControl(POWER_GPS/POWER_NFC, false)`) right after boot, since the app
  uses none of them.
