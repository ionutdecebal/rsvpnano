# RSVP Nano USB speed test in one HTML file

Open `RSVP-Nano-USB-Speed-Test.html` in desktop Chrome or Edge. Connect the **Waveshare ESP32-S3 Touch LCD 3.49 rev1** with a USB data cable and leave the SD card inserted. Everything needed to run the browser interface is embedded: scripts, logo, diagnostic firmware, verified Study13 restore image, and license notices. No Python, PlatformIO, local server, or internet connection is needed to use the built HTML file.

**Test 02 fixes font selection.** The first diagnostic used Compact (index 3) for the Large test. This version selects Medium (43 px) and Large (52 px) by catalog name, records font metadata and refuses a pass for mismatched/missing sizes. Reinstall the included diagnostic before rerunning; refreshing this page alone leaves the old firmware on the Nano. Old logs remain readable and explicitly flag Compact instead of claiming a Large-font pass.

1. Select **Enter flash mode**, choose the Nano, then select **Install diagnostic** and choose it again after USB reappears. This installs temporary benchmark firmware. If already installed, skip this step.
2. Wait for the Nano drive to appear. Select **Choose Nano drive** and grant access to the drive itself. The page checks the benchmark/board marker before writing only `benchmark/reader-1000.rsvp` and `benchmark/.run-ready`.
3. **Safely eject the drive in your file manager.** The device restarts. Within two minutes select **Connect & run test**, choosing the Nano’s serial connection. Keep the page open and the laptop awake during the two one-minute sessions.
4. Save the report and raw log. Then **Enter flash mode → Restore Study13** to get back to the normal reader. Both images are installed as separate sections, leaving NVS/settings at `0x9000–0xdfff` untouched. Full-chip erase is disabled.

Close other serial monitors/companion connections before connecting. If flash mode does not appear, hold BOOT, tap RESET, then release BOOT. A browser or operating system may request permission to access the serial device or write to the selected drive. If the drive does not mount, mount it in the file manager first. If the two-minute connection window expires, press RESET and repeat steps 2–3. A repeat run also starts at step 2. `Stop collecting` closes the laptop connection; the device may finish its current test independently. Reloading the HTML clears the in-memory log, so save results first.

The **Open log** control can inspect a previous `.log` without connecting hardware. Imported results are explicitly labelled and are not proof of the installed firmware identity. The SHA-256 shown in build information identifies the diagnostic bundled in this page.

## Measurement scope

The browser controls the test; the Nano's own clock measures it. No laptop animation or simulated timing is used for the result. See [the benchmark guide](../../docs/usb-reader-benchmark.md) for pass criteria and limits. This is the same diagnostic image as the CLI benchmark, with normal and largest built-in Literata fonts at 1,000 WPM, phantoms and a changing Focus badge. It excludes other fonts/scripts, chunk mode, full-app background work and physical panel scanout.

## Building and checking the artifact

The build requires Node.js, the benchmark `firmware.factory.bin`, and an extracted verified Study13 package with its `build-info.json` and `licenses` directory. Runtime users need only the output HTML.

```sh
cd benchmark/web
npm ci
npm run build -- --restore /path/to/Study13/rsvp-nano-lcd349-rev1-study13-2026-09-23.bin
npm test
npm run test:browser
```

Optional build arguments: `--diagnostic /path/to/firmware.factory.bin`, `--output /path/to/output.html`. Default output is `dist/RSVP-Nano-USB-Speed-Test.html`. The builder verifies image hashes/ESP32-S3 headers and identical diagnostic/restore partition tables. The restore image must match the Study13 package manifest. Dependencies are pinned in `package-lock.json`; scripts and firmware have no runtime CDN dependency.

Core tests cover malformed/missing/duplicate/late/skipped results; serial fragments, ANSI colors, disconnect, cancellation and timeout; fixture scope and failed writes; embedded image integrity; and NVS exclusion. Browser checks cover file URL support, layout/themes, imports/downloads, fake USB/file flows, installation/restoration orchestration and wrong-chip refusal. A disposable browser artifact substitutes the flasher for verification only; it is never distributed. Screenshots and test artifacts are stored in `build/html-usb-benchmark/`.

The HTML was checked locally with Chromium. The user successfully ran the original USB workflow on a Nano and supplied its log; Medium and Compact completed at the target speed. The corrected Large-font diagnostic has compiled and passed host checks, but still needs its own device run.

Browser API references: [Chrome Web Serial](https://developer.chrome.com/docs/capabilities/serial), [Chrome File System Access](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access), [Espressif esptool-js](https://espressif.github.io/esptool-js/docs/index.html). Implementation uses the locally inspected, pinned `esptool-js` 0.7.0 API.
