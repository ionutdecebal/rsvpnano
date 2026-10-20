# Candidate 3: device feedback pass

Target: LCD 3.49 Rev1. Local candidate tag: `preview-ui-dictionary-20261020.3`.
This is a manual browser-flash candidate, not a public firmware release.

## Changes

- Remove the Menu label and touch target from the reading screen. Middle button
  → quick controls → Menu remains the path to the main menu.
- Give the online dictionary onboarding explanation sufficient height in all
  three introduction languages; keep the full privacy explanation.
- Use the selected book family's smooth Small strike for modern page browsing.
  Restore the configured RSVP size even when returning to the exact same word
  while paused. Invalidate page layouts when another font role replaces a
  family's mutable strike, including Appearance and cache releases. Do not run
  chunk preparation while browsing a page.
- Generate consistent, antialiased navigation arrows with matching stem/head
  weight and text alignment. This adds no per-frame rendering work.
- Hide guides completely in chunk mode. Explain their availability in Word mode
  on the Guides tab and preserve existing size/gap preferences.
- Respect the pause-mode preference on hold-to-read release. Enforce a requested
  sentence stop within every reading-loop catch-up step, including when work
  crosses a word deadline after the initial UI check. Treat common two-letter
  sentence endings as words rather than automatically treating them as initials.

The font-selection and playback methods now live in focused production files so
host interaction tests can execute them instead of replacing them with stubs.
No new settings keys, partitions, online services or deferred ecosystem features.

## Validation

- 152 native tests passed: pacing, modern UI and general UI. Added checks for
  catch-up stops, deadline boundaries, chunk stops, short endings, manual resume,
  and complete onboarding/hint text in English, German and Romanian.
- The production-raster harness exercises the real playback/control methods and
  the real font-selection methods with a simulated mutable per-family font.
  Passed 24 repeated page/paused-reader round trips across four sizes in Word
  and Chunks, and real hold release, catch-up, prefetch-deadline and manual-resume
  checks. Hardware, SD catalog, persistence and prefetch I/O remain fixtures.
- 182 production-renderer captures regenerated with no missing screen routes.
  Inspected the dictionary lesson in all three languages, chunk-guide settings,
  page typography and reader chrome. Offline review-board browser checks passed:
  pins, safe text, persistence, export/import, deduplication and mobile width.
- Preview release-source routing checked: default preview tag, explicit tags,
  custom owner/repository and intentional Classic selection.

Final raster/build/artifact results are recorded with the packaged candidate.
These host checks do not establish physical touch, SD rendering speed, 1000 WPM,
long-book stability or successful device installation.

## Device retest

1. Tap and hold playback with Instant and Sentence end. Try a short word at the
   sentence end and stop during fast reading; confirm manual resume.
2. Repeated page/paused-reader round trips in Word and Chunks, all font sizes;
   return without moving position and after opening Appearance from page mode.
3. Page scrolling and selected font readability; arrow legibility on the panel.
4. Chunks have no guides at all. Guides tab explains why, and Word mode restores
   the prior guide size/gap. Reader has no Menu button; quick controls still does.
5. Replay the final dictionary lesson and confirm Look online text is complete.

Source is backed up on the preview branch; public firmware publication still
awaits device acceptance. Classic and the existing development OTA feed remain
unchanged. The full ecosystem prototype remains in its separate preserved tree.
