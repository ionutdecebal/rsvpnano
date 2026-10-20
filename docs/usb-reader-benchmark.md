# Run the current reader benchmark over USB

## Browser option

For the UI + dictionary candidate, use the source-based USB runner below and restore the normal candidate with its supplied merged image afterward. The older **RSVP-Nano-USB-Speed-Test.html** is a historical package: it contains a Study13 restore image and must not be treated as a benchmark/restore bundle for the current candidate. Repackaging that graphical tool with a matching diagnostic and candidate restore image remains a separate step. See [the browser tool documentation](../benchmark/web/README.md) for its historical scope.

## Command-line option

For the Waveshare ESP32-S3 Touch LCD 3.49 rev1, connect the device with a USB data cable and leave its microSD inserted. Close other serial monitors or companion connections, then run:

```bash
cd /home/john/Projects/rsvpnano
python3 benchmark/run.py --reader-only
```

The command reuses installed PlatformIO, builds and uploads temporary diagnostic firmware, copies a small generated test book through the device's USB storage connection, starts the test over USB serial, and saves the report automatically. It does not sync font packs or run the full conversion benchmark. On this computer the existing PlatformIO installation is detected without `uvx`.

If several devices are connected, add `--port /dev/ttyACM0` (use your device's actual port; on Windows this might be `COM3`). If USB storage is not mounted automatically, mount it and add `--sd-root /path/to/mounted/device`. The script checks the benchmark marker before copying files. `--manual-eject` lets you eject the volume through your file manager instead of the script requesting a reboot after copying.

The test itself takes about two minutes, plus building, uploading and preparing the book. Keep the USB connection in place until the report appears. It waits for a serial start command so testing cannot finish before the computer connects.

## What it measures

- Two 1,000-word sessions in Word mode at 1,000 WPM, using built-in Literata **Medium (43 px, index 1)** and **Large (52 px, index 0)**. The firmware resolves the catalog names and logs the chosen index, name and pixels-per-em in each result.
- The current 640 × 172 reading UI, phantom words, visible chapter/progress controls, and a changing Focus badge driven by a real Focus timer.
- Words read through the normal indexed SD book path. Generated numbered words have varied lengths and distinct content to exercise actual redraws.
- Normal reading timing and catch-up behavior, with length, complexity and punctuation delays at zero. The test never forces the reading clock forward.
- Reader update, drawing and display flush time, including the initial frame and idle cycles that refresh the badge. Serial metrics are emitted after each timed session.
- Submitted word count, skipped word indices, longest interval between new words, and total playback time. These prevent a fast result obtained by skipping words.

A session passes only if the recorded font size matches the intended case, all 1,000 words are submitted in order, no measured reading cycle exceeds 60 ms, playback completes, and elapsed playback is within 120 ms of one minute. The report also shows the longest word-to-word gap; a pass does not mean perfectly uniform display intervals.

**Test 02 correction:** the first diagnostic incorrectly selected index 3 (Compact, 14 px) for the Large case. Those logs still contain useful Medium/Compact timing data, but cannot certify Large. The corrected report flags that mismatch. Reopen the updated HTML and **reinstall its diagnostic**, then prepare and run again. Merely reloading the HTML does not update device firmware.

## Results and returning to the reader

The terminal prints `1000 WPM: PASS` or `FAIL / INCOMPLETE`. The easy-to-read report is:

```text
benchmark/logs/<timestamp>-<environment>.reader.md
```

The matching `.log` retains the raw results. Missing cases, incomplete serial output, skipped words, missed deadlines or incorrect elapsed time cannot produce a pass. A failed or incomplete reader result gives the command a nonzero exit status.

Reinstall the normal **Study 13** firmware after testing, using the usual installer with **Erase existing settings first OFF**. The diagnostic replaces the app temporarily; it does not provide the normal library/menu experience. The existing Study 13 ZIP in Downloads is unchanged. If diagnostic firmware is already installed, use `--reader-only --no-upload` for another run.

## Scope

This is an on-device reader/rendering check, not a laptop rendering estimate. It covers these two Latin, built-in-font sessions and the production reader/SD/display path. It excludes the full App loop's Wi-Fi, battery polling and settings/progress persistence; benchmark mode suppresses progress writes. It does not certify other SD fonts, multilingual shaping, chunk-mode speed, or the panel's physical scanout. A submitted word is counted after the display flush returns. Hardware results are available only after a real device run.

The separate, older full-suite reading benchmark also now selects the modern reading palette for the 640 × 172 Word-mode cases. Legacy page-rendering checks in that suite keep their original mode.
