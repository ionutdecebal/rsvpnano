# Hardware audit

Compared the supported board implementations with revision-matched schematics, Waveshare
examples/BSPs, and controller documentation. The supplied report was a checklist, not evidence
that every proposed register change was correct. Physical verification remains separate from
host tests and firmware builds.

## Confirmed corrections

| Hardware | Correction | Evidence |
|---|---|---|
| AMOLED 1.8 V1/V2 | Correct EXIO0 LCD reset, EXIO1 display enable, EXIO2 touch reset, EXIO7 SD-CS. Recovery resets touch alone; failed startup sequencing propagates an error. | [1.8 schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8/ESP32-S3-Touch-AMOLED-1.8.pdf) |
| AMOLED 1.8 V1/V2, 2.06, 2.16; C6 LCD 1.47 | Retain falling-edge touch notifications and wake the existing sampler. Consume pending work before I2C; preserve events during reads and retry failed reads. | Board-matched Waveshare Arduino touch examples, including the exact AXS5106L implementation for C6 |
| AMOLED 2.16 | Replace the unrelated CST816-family `FA 40` initialization with the CST92xx identification sequence. Reject corrupt markers/counts/IDs without manufacturing a release. | [2.16 SensorLib source](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/tree/main/examples/arduino/libraries), [Waveshare components](https://github.com/waveshareteam/Waveshare-ESP32-components) |
| C6 LCD 1.47 | BOOT and its wake source are GPIO9, not GPIO8. | [C6 schematic](https://files.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47/ESP32-C6-Touch-LCD-1.47-Schematic.pdf); the overview pin table conflicts with the circuit |
| AMOLED 1.8, 2.06, 2.16 | Establish DCDC1 at 3.3 V; enable/check ALDO1 at 3.3 V before using ES8311 audio. XPowers initialization alone does not set these rails. | [1.8 power configuration](https://github.com/waveshareteam/waveshare_boards/blob/main/boards/esp32_s3_touch_amoled_1_8/power_manager.c), [2.06 schematic](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/blob/main/Schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf), [2.16 schematic](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/blob/main/schematic/ESP32-S3-Touch-AMOLED-2.16-Schematic.pdf) |
| AMOLED 2.41 | Label the existing firmware V1, including installer/export labels. It must not be presented as V2-compatible. Asset identifiers remain unchanged. | [Revision differences](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.41#v1-vs-v2-differences) |

The 1.8 expander has no concurrent runtime output writers in the current call graph: system/display
initialization precedes the input task, and touch recovery is its only runtime writer. Each reset
edge reads the current latch/direction state. No additional mutex or hardware wrapper is needed.

Touch IRQ handlers are detached while the sampler is paused. Light-sleep cleanup disables the
normal GPIO interrupt trigger as well as its wake source: ESP-IDF's wake setup changes that trigger
to level-sensitive, and a later Arduino `pinMode` can otherwise re-enable it. The app finishes its
wake-contact reads before resuming the sampler, avoiding concurrent touch reset/read operations.

The pending flags use only aligned 32-bit atomic loads/stores. These compile to native instructions
on the installed ESP32-S3 and C6 toolchains. Do not substitute exchange/fetch/CAS: the installed S3
framework disables hardware read-modify-write atomics, even though word loads/stores remain native.

## Aligned rendering and orientation

The existing UI primitives compose complete opaque regions into one reusable native Arduino_GFX
two-row canvas before sending aligned rectangles. There is no full-screen framebuffer, panel
readback, retained draw-command list, or second UI model. The existing LCD 3.49 AXS15231B
canvas/row-prefix implementation and C6 direct drawing path do not compile this strip buffer.

The panel controllers impose restrictions independently of SPI DMA alignment:

| Controller | Documented update window | Existing supported boards |
|---|---|---|
| SH8601 | Even X/Y starts and even width/height; window and image dimensions must agree (pp. 107, 109). | AMOLED 1.8 V1 |
| CO5300 | Even starts and even spans (pp. 159, 161). | AMOLED 1.8 V2, 2.06, 2.16 |
| RM690B0 | Even starts and even spans (pp. 73-74). | AMOLED 2.41 V1 |

Sources: [SH8601 datasheet](https://files.waveshare.com/wiki/common/SH8601A0_DataSheet_Preliminary_V0.0_UCS_191107_1.pdf),
[CO5300 datasheet](https://dl.espressif.com/AE/esp-iot-solution/CO5300_Datasheet_V0.00.pdf),
[RM690B0 datasheet](https://files.waveshare.com/wiki/common/RM690B0_DataSheet_V0.3_20210105_(Public_version).pdf).

Arduino_GFX forwards arbitrary primitive windows. Rounding their coordinates alone would supply
the wrong number of pixels or overwrite neighbours. The remote V2 diagnostic showed missing
one-row/one-column primitives and corrupt odd-height transfers; the working aligned reference
does not establish that arbitrary UI drawing works.

### What the linked projects actually do

- [Pixelcat](https://github.com/toddsherman/pixelcat/blob/74af7d274de045493529d4b790f5f9bb9d5bd976/main/display.c#L31-L37)
  uses two 368 x 28 RGB565 DMA bands, 41,216 bytes total, not a full framebuffer. Each band is
  composed before its complete aligned transfer. Its drawing code supplies all the band's pixels.
- [Buddy](https://github.com/vthinkxie/claude-desktop-buddy-esp32/blob/61a0ce9f7410ed87de2032f226e511c9e59abbfe/src/hw/display.cpp#L119-L229)
  uses an Arduino canvas. Its 1.8 target is SH8601 V1, with an 184 x 224 canvas upscaled using
  one-row writes. Those writes conflict with the documented SH8601 restriction; they do not prove
  that CO5300 V2 supports them. Its CO5300 letterbox path describes per-row failures and uses a
  physical-frame buffer.
- [Waveshare BSP 2.0.3](https://github.com/waveshareteam/Waveshare-ESP32-components/blob/9f4030c6e5cb888ad4cc268bfa7584c93ad53e30/bsp/esp32_s3_touch_amoled_1_8/esp32_s3_touch_amoled_1_8.c#L388-L416)
  rounds LVGL invalidated areas **before** LVGL draws the matching pixels. Partial buffers are
  sufficient. The [V2 16-pixel offset](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_8/versions/2.0.3/readme)
  already matches this firmware; 2.06's 22-pixel offset also matches the vendor implementation.
- The Arduino_GFX maintainer [recommends Canvas for this symptom](https://github.com/moononournation/Arduino_GFX/issues/780#issuecomment-3969734606).
  That workaround does not establish that hardware readback is impossible.

### Strip composition

The existing tester videos establish successful aligned two-row transfers. Widgets now own aligned
rectangles and repaint all pixels within them, including the background. Changed page highlights
recompose the affected ink bounds with neighboring words; shaping, pagination and input handling
are not replayed for each strip. Existing font objects and caches are reused. Standby animations
advance once per update and keep their existing dirty-cell tracking.

| Board | Default UI size | Two-row RGB565 allocation |
|---|---|---|
| AMOLED 1.8 V1/V2 | 448 x 368 | 1,472 bytes |
| AMOLED 2.06 | 502 x 410 | 1,648 bytes |
| AMOLED 2.16 | 480 x 480 | 1,920 bytes |
| AMOLED 2.41 V1 | 600 x 450 | 1,808 bytes |
| C6 LCD 1.47 | 320 x 172 | None |

The row pitch rounds up to four pixels for Arduino_Canvas's allocation alignment. Transfer payloads
are packed to the actual window width. AMOLED panels remain at native rotation zero; the native
canvas rotates each strip and the existing touch transform follows the same orientation. The
left-handed setting selects the opposite landscape orientation. Board configuration selects the
path at compile time; the regular LCD 3.49 layout is unchanged.

Arduino_GFX is pinned to the audited revision. Its u8g2 decoder used unsigned temporary coordinates,
which dropped glyph fragments crossing the strip edge. `tools/pio_gfx.py` builds a generated copy
with signed coordinates; it does not modify the dependency cache. Full logical text bounds let
the native canvas clip scaled glyph pixels correctly. The native-library regression compares real
u8g2/Canvas output against a full-canvas reference, independently of the UI test double.

Panel read-modify-write was investigated but is not used: it adds serial reads,
turnarounds and controller-specific uncertainty to drawing that can use small write-only strips.
[SiFli's CO5300 read implementation](https://github.com/OpenSiFli/SiFli-SDK/blob/575742f27c2c133421fe6759e952bdaad766c417/customer/peripherals/display/co5300/co5300.c#L413-L469)
is a real implementation lead on another MCU, not a verified Waveshare solution. No additional
readback diagnostic is needed to re-establish the already-observed aligned-transfer behavior.

## Deliberate differences and unresolved evidence

- Keep the contaminated LCD 3.49 touch-IRQ connection on its established polling path. AMOLED
  2.41 V1 also polls: touch IRQ is EXIO2 and the expander's interrupt output is unconnected.
- CST9220 post-read acknowledgement differs between Arduino (`D0 00 AB`) and BSP references.
  No unconditional ACK was added without exact-controller evidence or a physical trace.
- Do not copy 2.16's `MADCTL=0xA0` independently of the reference's paired touch transform.
  Arduino_GFX's CO5300 mirror constants also differ from the datasheet. Strip rotation keeps
  the panel at rotation zero, without relying on those unverified mirror values.
- Existing display sleep delays are not proven wrong merely because another example uses longer
  delays or avoids sleep. Charging, other PMU rails, minimum brightness, output-only audio and
  accelerometer-only operation are unchanged.
- I2S, IMU and storage pin assignments matched the inspected schematics. Feature differences
  from demonstration projects are not wiring defects.

## Physical acceptance checks

Build/host tests cannot replace these checks on each affected revision:

- Cold boot after power removal; read back system/audio rail configuration and confirm audio output.
- Touch tap/drag/stationary hold/release, a pulse between sampling deadlines, failed-read retry,
  recovery reset, and repeated standby wake without an interrupt storm.
- On 1.8, reset touch while display and SD remain undisturbed; verify only EXIO2 toggles.
- On C6, verify BOOT actions and BOOT/touch wake.
- Test the regular firmware's menus, keyboard, RSVP/page/CJK reading, chapter changes, focus timers
  and standby animations. Check both handedness settings, corner touch targets, partial updates
  and scrolling. Confirm redraw speed and readability on the physical display.

## Local verification

- All 252 native cases passed across the regular UI, watch UI, aligned-rendering, board-wiring,
  light-sleep and existing firmware suites. The final rendering recheck includes exact transfer
  counts, clipped/odd regions, touch rotation, incremental highlights, keyboard invalidation,
  screensavers and moved/removed widget ownership.
- The native Arduino_GFX regression passes 192 pixel comparisons; the original unsigned decoder
  fails 162 of those cases. Run `python test/native_canvas/run.py --fetch-library`.
- `checkWeb` and the production installer bundle passed. Committed conflict markers and a duplicate
  import in the USB regression test were removed without discarding its book-deletion check.
- The firmware-export script passed its syntax check.
- No physical board tests were performed during this audit.
