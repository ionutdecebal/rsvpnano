# Candidate 2 — review follow-up

Source feedback: the founder's 20 October review export (eight comments).
Candidate: `preview-ui-dictionary-20261020.2`, LCD 3.49 Rev1.
All requested changes are implemented; physical acceptance is still pending.
The original feedback export remains unchanged and its screen IDs are retained.

| Screen | Change | Verification |
| --- | --- | --- |
| Add books | Align each number with the first line, including wrapped text | Native captures, three languages and two themes |
| Physical buttons | Case and screen proportions follow the Waveshare drawing; remove orientation caption | Native captures; actual enclosure still needs review |
| Appearance · text | Font and size buttons have equal widths | Native capture and existing bottom-edge touch path checks |
| Introduction · page | Flow words using measured widths and spaces; retain word selection and scrolling | Native capture; device interaction pending |
| Quick controls | Menu/Page/Dictionary left, WPM/Back to book centre, brightness right | Native routing and slider tests |
| Reader controls | Middle button opens controls; remove swipe-up and handle; labelled Menu opens full menu | Native touch routes; hardware button check pending |
| Touch test | Move entry and return path to Display | Native route test and capture |
| Wi-Fi | Remember up to five networks, show Saved list, forget one, select available saved Wi-Fi for online features | Secrets migration/round-trip tests and real connection helper with fake radio/clock under ASan/UBSan |

The Wi-Fi helper checks available saved networks, bounds connection attempts and
retains credentials after failed scans. The companion chooses a visible saved
network at startup while retaining its direct access point. Hardware switching
between hotspot and home Wi-Fi remains part of device acceptance.

Host verification is not a touchscreen, radio, OTA reboot, 1000 WPM or long-book
stability result. The device acceptance gate in the main preview document remains.

The founder requested this candidate through the existing development OTA feed.
That is a migration from the full prototype to the reduced preview: this candidate
contains the GitHub updater only. The development APK feed stays unchanged.
No public GitHub firmware release or installer deployment is part of this pass.
