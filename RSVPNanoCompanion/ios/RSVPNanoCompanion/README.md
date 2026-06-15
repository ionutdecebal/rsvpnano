# RSVP Nano iOS Companion

Native SwiftUI companion app and share extension for RSVP Nano.

The iOS UI stays native. Shared business logic comes from Kotlin Multiplatform modules: `:shared`
owns models, API access, persistence, RSS/article workflows, and device orchestration;
`:conversionCore` owns document conversion.

## Requirements

- macOS with Xcode.
- JDK 17 for building the Kotlin shared framework.
- An Apple developer team for device signing.
- An iPhone for realistic app/share-extension testing.

Free Apple IDs can install development builds on personal devices, but provisioning can expire
quickly and may not support every share-extension/App Group capability. TestFlight/App Store
distribution requires the Apple Developer Program.

## Build Shared Framework

Build the Kotlin Multiplatform XCFramework from the repository root:

```bash
bash RSVPNanoCompanion/tools/build_shared_xcframework.sh
```

The script writes:

```text
RSVPNanoCompanion/ios/RSVPNanoCompanion/SharedFrameworks/shared.xcframework
```

The iOS project expects that framework to be embedded and signed by Xcode. If Xcode loses the
reference, add `SharedFrameworks/shared.xcframework` back to `Frameworks, Libraries, and Embedded
Content` for the app target and set it to `Embed & Sign`.

## Open In Xcode

Open:

```text
RSVPNanoCompanion/ios/RSVPNanoCompanion/RSVPNanoCompanion.xcodeproj
```

Use the `RSVPNanoCompanion` scheme for the main app. Check signing for both targets:

- `RSVPNanoCompanion`
- `RSVPNanoShareExtension`

Both targets must use the same App Group. The default group is:

```text
group.com.rsvpnano.companion
```

If you need a personal bundle ID or App Group, update all matching locations:

```text
RSVPNanoCompanion/ios/RSVPNanoCompanion/RSVPNanoCompanion/RSVPNanoCompanion.entitlements
RSVPNanoCompanion/ios/RSVPNanoCompanion/RSVPNanoShareExtension/RSVPNanoShareExtension.entitlements
RSVPNanoCompanion/ios/RSVPNanoCompanion/RSVPNanoCompanion/Models.swift
```

## Run On Device

1. Connect an unlocked iPhone over USB.
2. Trust the Mac if iOS prompts.
3. Enable Developer Mode if iOS prompts:
   `Settings -> Privacy & Security -> Developer Mode`.
4. Select the connected iPhone as the Xcode run destination.
5. Build and run the `RSVPNanoCompanion` scheme.
6. If iOS blocks launch, trust the developer profile:
   `Settings -> General -> VPN & Device Management`.

## CI Expectations

The macOS CI workflow compiles the converter for device iOS, builds the Kotlin XCFramework, and
uploads the generated framework artifact. CI validates the shared iOS framework path, but real
app/share-extension behavior still needs Xcode and device testing.

Run this locally on macOS when touching shared/iOS integration:

```bash
./gradlew checkIos --no-daemon
bash RSVPNanoCompanion/tools/build_shared_xcframework.sh
```

## Connect To The Reader

1. Put the reader into `Companion sync`.
2. Join the `RSVP-Nano-xxxxxx` Wi-Fi network shown on the reader in iPhone Settings.
3. Return to the app.
4. The app checks `http://192.168.4.1` automatically.
5. If the default address is not reachable, enter the address shown on the reader.

The app cannot change the reader firmware UI. The firmware currently shows the AP name and
`http://192.168.4.1` while Companion sync is active.

## Current Capabilities

- List, upload, and delete books/articles on the reader.
- Read and save reader settings.
- Read, save, and clear reader Wi-Fi settings.
- Add local RSS feeds and sync them to the reader.
- Save text/article drafts locally.
- Fetch URL-only article drafts.
- Sync saved articles to the reader.
- Import `.rsvp`, `.epub`, `.txt`, `.md`, `.markdown`, `.html`, `.htm`, and `.xhtml` files.
- Save incoming URLs or selected text from the iOS share extension.

## Share Extension

From Safari or another app, share a URL or selected text to `RSVP Nano`. The extension saves a local
draft through the shared module. Open the companion app later, connect to the reader, then sync saved
articles.
