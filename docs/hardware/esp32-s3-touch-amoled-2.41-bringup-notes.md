# ESP32-S3 Touch AMOLED 2.41 Bring-Up Notes

## Targets

| Revision | PlatformIO environment | Installer ID | OTA asset |
| --- | --- | --- | --- |
| V1 (no Rev2.0 marking) | `waveshare_esp32s3_touch_amoled_241` | `amoled241` | `rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin` |
| V2 (Rev2.0 PCB or V2 QC label) | `waveshare_esp32s3_touch_amoled_241_v2` | `amoled241-v2` | `rsvp-nano-esp32-s3-touch-amoled-2.41-v2-ota.bin` |

V1's existing environment, board ID, installer ID, and asset names remain unchanged.
V1 and V2 firmware are not interchangeable. Version headers under
`src/platforms/waveshare_amoled_241/v1/` and `v2/` select wiring at compile time.
Both compile the regular UI, not the watch UI.

The platform folder and common board facts remain under
`src/platforms/waveshare_amoled_241`. Drivers remain Arduino_GFX `Arduino_RM690B0`,
`src/drivers/touch/ft6336`, and `src/drivers/imu/qmi8658`.

## Revision wiring and TP_INT

| Signal | V1 | V2 |
| --- | --- | --- |
| OLED reset | GPIO21 | TCA9554 EXIO0 |
| Touch reset | GPIO3 | TCA9554 EXIO1 |
| TP_INT | TCA9554 EXIO2 | GPIO3, direct falling-edge interrupt |
| Display power enable | TCA9554 EXIO1 | No corresponding enable; EXIO1 is touch reset |
| OLED_TE | Not the V2 mapping | GPIO21; never drive as reset |

**V1 cannot provide an MCU touch interrupt without a hardware modification.**
TP_INT terminates at EXIO2, but the TCA9554 INT output is unconnected in the V1
schematic. EXIO2 is explicitly kept as an input. Report polling remains necessary;
polling the expander's live input would miss short pulses and is not a substitute
for a latched interrupt. V1 does not advertise touch light-sleep wake.

V2 attaches `FALLING` on GPIO3. The ISR only records a pending event and notifies the
input sampler; all I2C stays outside the ISR. An initial sample covers contact that
started before attachment. Pending is cleared before reading so an interrupt
arriving during I2C is retained, and failed reads stay retryable. Cancellation/end
removes the edge ISR for the sleep path, and touch initialization reattaches it.
The existing light-sleep path can use GPIO3 as a level-triggered touch wake source.
GPIO18 is V2's expander interrupt, not TP_INT.

V2 resets EXIO0/EXIO1 high for 20 ms, low for 20 ms, then high for 120 ms, matching
the vendor example. Each edge preserves the other output latches and directions.
Reset/expander failures propagate rather than claiming successful initialization.
The FT6336 keeps its reset defaults; the unrelated FT3168 monitor-mode setup is not
copied onto this board.

Sources:

- [Official revision wiring table](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.41#v1-v2-differences).
- [V1 schematic: EXIO2 and unconnected TCA9554 INT](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.41/ESP32-S3-Touch-AMOLED-2.41-sch.pdf).
- [Official V2 Arduino example: GPIO3 interrupt and expander reset sequence](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.41-V2/blob/0eaf16cfae3440d445f2b6caa71e94065094551f/02_Example/Arduino/09_LVGL_Test/09_LVGL_Test.ino).
- [Vendor touch driver: negative-edge GPIO configuration](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.41-V2/blob/0eaf16cfae3440d445f2b6caa71e94065094551f/02_Example/Arduino/09_LVGL_Test/esp_lcd_touch_ft5x06.c).

## Common hardware

- Panel native geometry: `450x600`; regular UI: `600x450` landscape.
- Touch controller: FT6336; display controller: RM690B0.
- I2C: GPIO47/48; battery hold: GPIO16; battery ADC: GPIO17.
- SDMMC: GPIO4/5/6.

Previously recorded V1 behavior: display initialization, aligned touch, correct
colors, visible-panel alignment, stripe-free transfer chunking, and SD loading.
This is not evidence of a physical V2 test.

## Implementation and retained bring-up lessons

`BoardDisplayPower.cpp` owns revision-specific rail/reset sequencing.
`BoardDisplay.cpp` calls the existing RM690B0 driver. `BoardInput.cpp` exposes raw
controls and touch contacts; the shared input sampler owns debounce and gestures.
Power, SD, and IMU stay in their existing board implementations. The shared app
must not include chip-driver or platform headers directly.

Keep panel addressing at native rotation zero. The shared UI composes rotated
32-row strips for landscape, and touch uses the matching transform. Arduino_GFX's
RGB565 transfer already handles byte order; do not add another byte swap. The
16-pixel column offset belongs to panel addressing. Retain the board-specific
larger transfer chunk that fixed stripe artifacts on the rotated panel path.

The updater rejects obvious cross-board asset overrides by name. Embedded board
metadata would still be a stronger future guard. V2 filename matching must precede
the generic 2.41 match in the web installer.

## Validation

```sh
pio test -e native_amoled_241_v1_test -e native_amoled_241_v2_test
pio run -e waveshare_esp32s3_touch_amoled_241
pio run -e waveshare_esp32s3_touch_amoled_241_v2
python -m unittest discover -s tools -p 'test_firmware_targets.py'
```

Host regressions cover revision pins/identities, expander preservation, startup
failures, short interrupt pulses, interrupts during I2C, read retries, and ISR
cancellation/reattachment. They do not validate electrical timing or hardware.

Before merging/releasing, run the full firmware matrix and check both revisions on
hardware: cold boot, orientation, colors, brightness, all touch edges, dragging and
release, failed-read recovery, SD loading, battery reporting, soft-off and wake on
USB/battery, long reading sessions, and correct OTA asset selection. Verify V2
GPIO3 pulses and touch sleep/wake with a logic analyzer. V1 is expected to poll and
must not be reported as supporting touch IRQ wake. USB MSC remains disabled for
these targets; this change does not enable it.
