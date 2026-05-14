# RSVP Nano Companion

Native SwiftUI iPhone app for RSVP Nano companion sync.

The app is not required for the public firmware release. Android, desktop, and iPhone users can use
the device-hosted web companion from Companion sync. The native app adds the iOS share sheet flow,
local article drafts, and a more polished iPhone experience.

TestFlight/App Store distribution is planned. Until that is approved, installing from Xcode is the
temporary tester path.

## Install From Your Mac

This is a temporary no-TestFlight install path for owners, contributors, and testers who are
comfortable using Xcode.

1. Install Xcode from the Mac App Store.
2. Open Xcode and sign in:
   - `Xcode -> Settings -> Accounts`
   - Add your Apple ID.
3. Connect your iPhone to the Mac with USB.
4. Unlock the iPhone and tap `Trust This Computer` if prompted.
5. If iOS asks for Developer Mode, enable it:
   - `Settings -> Privacy & Security -> Developer Mode`
   - Restart the iPhone when prompted.
6. Open:

```text
ios/RSVPNanoCompanion/RSVPNanoCompanion.xcodeproj
```

7. Select the `RSVPNanoCompanion` scheme.
8. Select your connected iPhone as the run destination.
9. Select the project in Xcode, then check signing for both targets:
   - `RSVPNanoCompanion`
   - `RSVPNanoShareExtension`
10. In `Signing & Capabilities`, enable `Automatically manage signing` and choose your team.
11. If Xcode says the bundle identifier is unavailable, change both bundle IDs to something unique,
    for example:

```text
com.yourname.rsvpnano
com.yourname.rsvpnano.share
```

12. The app and share extension must use the same App Group. If you change it, update all three
    places:

```text
ios/RSVPNanoCompanion/RSVPNanoCompanion/RSVPNanoCompanion.entitlements
ios/RSVPNanoCompanion/RSVPNanoShareExtension/RSVPNanoShareExtension.entitlements
ios/RSVPNanoCompanion/RSVPNanoCompanion/PendingUploadStore.swift
```

For example:

```text
group.com.yourname.rsvpnano
```

13. Press `Run` in Xcode. Xcode builds, installs, and launches the app on the iPhone.
14. If iOS says the developer is not trusted, open:
    - `Settings -> General -> VPN & Device Management`
    - Trust your developer profile.

Free Apple IDs can run apps on personal devices, but they may have short-lived provisioning and may
not support every capability needed by the share extension. A paid Apple Developer Program account
is still required for TestFlight, App Store, and unlisted App Store distribution.

## Connect To The Reader

1. Put the reader into `Companion sync`.
2. Join the `RSVP-Nano-xxxxxx` Wi-Fi network shown on the reader in iPhone Settings.
3. Return to the app.
4. The library appears once the HTTP API at `192.168.4.1` is reachable.

The app can read `/api/info`, list `/api/books`, delete books, upload files, change device
settings, save Wi-Fi credentials for RSS/OTA, and manage RSS feeds. The library shows saved reading
percentage when the reader reports `progressPercent` for a book.

## Add Reading Material

- `Upload File`: pick `.rsvp`, `.epub`, `.txt`, `.md`, `.markdown`, `.html`, `.htm`, or `.xhtml`
  from Files. EPUB/Text/Markdown/HTML files are converted locally when possible; unsupported EPUB
  archives are uploaded as `.epub` so the reader can convert them on open.
- `New Text`: paste text or article extracts, optionally with a title/source, and upload the
  generated `.rsvp`.
- Share Extension: from Safari or another app, share a URL or selected text to `RSVP Nano`. The
  extension extracts and formats the article, converts it to `.rsvp`, and saves it into the app's
  pending inbox. Open the companion app later, connect to the reader Wi-Fi, and use `Sync Saved
  Articles`.
