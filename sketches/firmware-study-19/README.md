# Study 19 — Check your card before reading

HTML-only continuation of Study 18. The flashable Study 17 firmware is unchanged. Nothing has been pushed or published to GitHub.

## Onboarding change

After language selection and Show me, a short SD-card check appears before the ten existing lessons. It shows three results: Open card, Prepare folders, and Save & read back. Continue becomes available after a successful check; the screen waits for the user instead of disappearing automatically.

Failures offer Retry and Try demo. Demo practice uses the existing isolated passage. Completing or leaving the tour after a failed or interrupted check does not mark first-time setup complete. A simulated restart returns to the introduction until setup is completed with a working card. An already completed introduction is not erased by a later manual replay. The existing Skip tour option on Welcome still lets someone skip the entire introduction.

The result explains the next useful action. An unavailable card is described as a mount failure: the UI does not claim to distinguish a missing card from a bad format or hardware problem. Save failures mention free space as a possible cause without claiming the card is full. An empty library is not an error. A stalled check ends in an incomplete result rather than waiting forever.

The outside Card scenario selector and Try card check shortcut are preview controls only. Outcomes are simulated. This HTML does not read, write, repair or format any real storage. The fake short delays demonstrate status transitions, not measured firmware timing.

## Firmware basis and later integration

- `src/storage/fs/SdCard.cpp` already tries supported SD clock rates and verifies a written file, including matching the bytes read back and removing the probe.
- `StorageMigration::prepareLayout()` prepares the reader's folders. `src/storage/fs/SdDiagnostics.cpp` checks required directories and runs per-directory write/read probes.
- The existing diagnostic result is summarized text; the firmware pass needs structured progress/results to drive these three rows. Reuse startup results where appropriate instead of repeating the entire scan just to animate this screen.
- Missing, unsupported and damaged cards cannot reliably be distinguished by the current mount result. The preview deliberately groups them.
- Free-capacity reporting is not currently exposed by the board storage interface. No fabricated free-space number appears here.
- The future check must run through the background-job path, support bounded waiting and ignore late results after leaving. The browser generation guard models that lifecycle; it is not a firmware implementation.
- Use a uniquely owned temporary probe, never a book/config filename. Do not invoke the broader migration repair/cleanup flow merely to show this onboarding result. The interface offers no automatic formatting or deletion of user files.
- Persist first-run completion only after an appropriate successful result; if saving the flag itself fails, retain the pending save and report the limitation.

The book-upload QR, permanent Help → Add books entry, Wi-Fi/USB instructions, three languages and five themes from Study 18 remain available. The QR opens the web companion; it does not pair or transfer files. All demo connections remain simulated.

## Review and verification

Open the standalone HTML and press Show me, or use Try card check. Change the outside Card scenario selector to review working, unavailable, folder failure, save/read-back failure and stalled states.

```sh
python3 sketches/firmware-study-19/package.py build/study19/RSVP-Nano-UI-Preview-19.html
node sketches/firmware-study-19/inspect.cjs
node sketches/firmware-study-19/inspect-storage.cjs
```

The 14 inherited browser groups cover the complete introduction, gestures, pacing, Focus and upload guide. Six additional groups cover storage gating, correct stage results, demo completion/restart, retry/cancellation, ignored late callbacks, three-language text bounds and five-theme presentation. Reports and screen captures are in build/study19. Physical-card behavior still needs a firmware implementation and device validation.

The single-file HTML embeds scripts, fonts, logo and the existing static QR. It makes no external requests unless the user follows the companion link. Browser storage uses independent Study 19 keys.
