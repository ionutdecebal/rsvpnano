# LCD 3.49 Rev2 release delivery

On 23 October 2026 the founder accepted the Rev2 New UI hardware test, then
explicitly approved publishing the firmware, installer and on-device update choices.

- Rev1 New UI stays at immutable `v0.2.0` (`cf42168db427b4a0e96601d4f093313e5cbfb243`).
- Rev2 New UI is `v0.2.0-rev2.1` (`73af8bc7c4fdd4bd3c665a447f2fa2c5b3f8cf92`),
  the exact accepted local image. It remains a prerelease and is never Latest.
- Classic `v0.1.5` (`e07b4e8fb494a35531a262798bb024d9153d6e0f`) adds the Rev2
  selector and remains Latest. Its source is on `release/classic-rev2-switch`;
  the main-branch website changes do not replace Classic firmware sources.

Pages uses `RSVP_UI_PREVIEW_TAG=v0.2.0` and
`RSVP_UI_REV2_PREVIEW_TAG=v0.2.0-rev2.1`. The installer reads `newUiVariants`,
selecting each board's image and version independently. The old `newUi` field
is retained for cached Rev1 clients. Classic remains the default.

`firmware/channels.json` contains four entries: Classic and New UI for each LCD
revision. Staging verifies the OTA hash, length, embedded version and board asset
identity before exposing an entry. Never point Rev2 at a Rev1 binary.

Classic retains nine OTA files, two legacy aliases, one browser ZIP, checksums
and a descriptor: fourteen assets. Keep the release API body below the v0.0.8
updater's 32 KiB limit. Do not reintroduce nine individual merged release assets.
Do not move published tags or overwrite their files.

Validation includes all nine Classic builds, downloaded asset digests, merged/OTA
pairs, unchanged partitions, the actual v0.0.8 release parser, settings compatibility,
existing Rev1 and new Rev2 catalog readers, Python delivery tests and browser tests.
The owner's Rev2 hardware test passed; physical Rev2 OTA switching and a 1000 WPM
benchmark have not been separately reported. These are not established by host tests.
