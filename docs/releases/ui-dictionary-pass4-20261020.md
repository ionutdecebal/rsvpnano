# Candidate 4: menu and Library refinement

Target: LCD 3.49 Rev1. Local tag: `preview-ui-dictionary-20261020.4`.
Manual browser-flash candidate; public firmware publication awaits device acceptance.

## Changes

- Connections contains Wi-Fi, companion, USB and RSS. Dictionary lookup lives in
  quick controls; installation/online help is its **?** button. Returning from
  help preserves the lookup.
- Library's Book details button becomes Book settings. It edits the selected
  book's language and font overrides without opening that book in the reader.
  The existing reading position stays intact. Metadata preparation runs behind
  a loading screen, including indexing a previously unopened import if needed.
- Per-book edits use the existing SD state format. Override writes preserve
  position and use a temporary file/backup replacement; failed saves are
  reported and the UI reverts the unsaved change. Editing the active book also
  updates its live overrides and refreshes typography.
- Reading no longer has Book settings. Interface language is under Device;
  Display retains theme, standby and screensaver. Onboarding and review-board
  routes match the new locations.
- Page view retains the selected family's exact **Small** strike, as requested.
  Candidate 3 already chose Small. No Compact substitution or additional scaling
  is made. Its page/paused-reader font restoration fixes remain intact.

No partition changes, new settings keys, app/notes/Bluetooth features or OTA feed
publication. Classic remains the default stable firmware.

## Validation scope

Run the native pacing/modern UI/general UI suites, production-renderer capture
harness, storage regression harness, per-book settings harness and release-source
checks. The per-book harness uses production metadata/editor/persistence code with
injected filesystem/indexer hardware; it exercises current/other book isolation,
saved positions, reset/reload, missing sidecar, failed writes, unindexed imports,
indexing failure, invalid selections and unmounted storage.

The capture harness checks the real menu controls and Small page font selection,
repeated return to paused RSVP/Chunks, language cycling and Dictionary help
round trips. Review IDs remain stable: the former `library-details` ID now shows
the selected book's settings so existing comments remain importable.

Final test/build/image results are recorded below and in the package manifest.
Host checks do not establish physical touch, SD performance, 1000 WPM or a
successful installation.

## Device retest

1. Select a different Library book → Book settings. Change language/typeface,
   return to Library, reopen its settings and verify persistence.
2. Return to the current book and confirm its book/position did not change.
   Open the edited book and confirm its overrides apply at its saved position.
3. Edit the currently open book from Library and confirm the change applies on
   return without needing playback to refresh the font.
4. Device → Language: verify the UI changes and persists after restart.
   Display should have theme, standby, screensaver and Touch test.
5. Quick controls → Dictionary → **?** → back retains the lookup. Connections
   has no Dictionary duplicate.
6. Compare page view against RSVP's **Small** setting using the same book/font,
   then repeat page/reader transitions with other RSVP sizes while paused.

## Host results

- 152 native UI/pacing tests passed. All 31 modern UI tests passed again after
  the final header/language-label adjustments.
- Both per-book persistence regression groups passed, including failed opens and
  partial writes. Seven existing storage/cache regression groups passed.
- The production-renderer harness passed and regenerated 182 capture states,
  with zero missing screen routes. Inspected Library/Book settings, Connections,
  Device, Display, Reading, Dictionary/help and onboarding captures.
- Release-source routing checks passed. LCD 3.49 Rev1 firmware build passed.

The package records final tagged-image validation separately.
