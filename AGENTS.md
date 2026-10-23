# RSVP Nano UI and dictionary release

Read `docs/releases/ui-dictionary-preview.md` for scope and release requirements.
The founder approved this scoped implementation and GitHub source publication.
The complete ecosystem prototype is preserved separately; do not add the new
Android app, notes, recording, Bluetooth phone sync or private service data here.

Keep Classic available and the default stable release. New UI is an explicitly
selected preview for LCD 3.49 Rev1 and Rev2. On 23 October 2026, after accepting
the Rev2 hardware test and source push, the founder explicitly approved publishing
the tested Rev2 firmware, browser installer and bidirectional OTA choices.
Rev1 stays on v0.2.0; the accepted Rev2 binary is v0.2.0-rev2.1 at 73af8bc7c4fd.
Classic v0.1.5 adds the Rev2 selector and remains the default stable release.
Read `docs/releases/lcd349-rev2-candidate.md`. Use unique release tags and separate merged
browser and application-only OTA artifacts. Never move published tags, overwrite
assets or change flash partitions. Preserve SD books, progress and deferred data.

Generate review images from production renderers with synthetic hardware/data
fixtures. Preserve screen IDs and exported comments across board regeneration.
Host builds and captures do not establish physical touch, OTA or reading speed.
Public firmware publication follows candidate review and actual device acceptance.
Keep installer changes separable from firmware changes to the stable branch.

The founder approved publishing English dictionary data on GitHub independently
of firmware. The data release must not become Latest or change the firmware update
feeds. English only for now; the UI should say more languages are planned.
