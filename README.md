# RSVP Nano (Waveshare ESP32-S3 Touch AMOLED 1.91)

Minimalist one-word-at-a-time RSVP reader MVP firmware.

## Repository inspection summary

This repository started empty (only `.gitkeep`), so the MVP includes a full first implementation:

- **Build system**: PlatformIO (`platformio.ini`)
- **Framework**: Arduino for ESP32-S3
- **Board support code**: Added in `BoardSupport` and `BoardConfig`
- **Display/touch/storage integration**: Added modular integrations for display (Adafruit_GFX + ST7789), touch (I2C CST816-style polling), and storage (SD_MMC)
- **Entry points**: `src/main.cpp` and `App` state machine (`src/App.cpp`)

## Features implemented (MVP scope)

- Hold **BOOT** to play RSVP; release pauses with ~200ms release buffer
- Touch is ignored while playing
- Paused-only gestures:
  - Horizontal scrub (sentence/paragraph-aware based on gesture magnitude)
  - Vertical swipe adjusts WPM in steps
  - Long press opens minimal menu
- Triple press BOOT while paused enters sleep
- Minimal menu (exactly): Resume, Chapters, Restart book, Change book, Sleep
- Plain text book loading from SD card `/books/*.txt`
- Per-book progress persistence + WPM persistence
- Chapter detection heuristics and chapter jump
- Subtle pause transition via restrained dimming

## Build

```bash
pio run
```

## Flash

```bash
pio run -t upload
```

(Optional serial monitor)

```bash
pio device monitor -b 115200
```

## Controls

### Playing
- **Hold BOOT**: play words continuously
- **Release BOOT**: pause (with ~200 ms forgiving buffer)
- Touch gestures disabled while playing

### Paused
- **Horizontal drag/swipe**: scrub
  - smaller motions: finer word-level corrections
  - medium motions: sentence boundary jumps
  - larger/faster motions: paragraph-aware jumps
- **Vertical swipe**: WPM changes in stepped increments
- **Long press touch**: open menu
- **Triple press BOOT**: sleep

### Menu
Exactly five options:
1. Resume
2. Chapters
3. Restart book
4. Change book
5. Sleep

## Storage layout / book format

- SD card mounted with `SD_MMC`
- Put books in: `/books/*.txt`
- Progress/settings stored under:
  - `/rsvp/settings.json`
  - `/rsvp/progress/<book-id>.json`

Books are plain UTF-8-ish text (ASCII-safe expected for MVP).

## Board-specific assumptions and mappings

Because the original repo had no existing board package/examples, this MVP uses practical assumptions in `include/BoardConfig.h`:

- BOOT button on GPIO0
- AMOLED uses an SPI-style fallback configuration through Adafruit ST7789
- Touch uses I2C CST816-style register polling (address `0x15`)
- Screen logical size: `536x240`

> Important: Waveshare board revisions may differ for panel controller and pin mapping. Update `BoardConfig.h` and display class in `BoardSupport::initDisplay()` to match your exact vendor example if needed.

## Sleep behavior

- Triple BOOT press while paused enters deep sleep
- Wake source configured on BOOT GPIO low via ext0
- RESET is not used for app UX (hardware reset only)

## Limitations / notes

- Since no existing board demo code was present in the repository, hardware mappings are best-effort assumptions.
- Display driver currently uses a generic ST7789 fallback init to keep the project modular/compilable; swap to the exact Waveshare panel init if your board uses a different controller.
- Display layer includes a compatibility fallback stub so the project still compiles if Adafruit display headers are unavailable in a given environment; replace with board-vendor driver on hardware.
- Touch parser is CST816-like; if your board uses GT911/CST226/etc., replace only the touch-read internals while preserving `TouchEvent` contract.

