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

## Kotlin Framework Integration

The Xcode project uses Kotlin Multiplatform direct integration. Its `Compile Kotlin Framework`
build phase runs `:shared:embedAndSignAppleFrameworkForXcode` for the selected device or simulator.
There is no generated XCFramework to copy into the repository.

## Open In Xcode

Open:

```text
RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/RSVPNanoCompanion.xcodeproj
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
RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/RSVPNanoCompanion/RSVPNanoCompanion.entitlements
RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/RSVPNanoShareExtension/RSVPNanoShareExtension.entitlements
RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/RSVPNanoCompanion/Models.swift
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

The macOS CI workflow runs the shared iOS checks, then uses `xcodebuild` to build the app and share
extension for an Apple Silicon simulator without signing. Real app and share-extension behavior
still needs device testing.

Run this locally on macOS when touching shared/iOS integration:

```bash
./gradlew checkIos --no-daemon
xcodebuild \
  -project RSVPNanoCompanion/apps/ios/RSVPNanoCompanion/RSVPNanoCompanion.xcodeproj \
  -scheme RSVPNanoCompanion \
  -sdk iphonesimulator \
  -destination 'generic/platform=iOS Simulator' \
  ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGNING_ALLOWED=NO build
```

## Connect To The Reader

1. Put the reader into `Companion sync`.
2. Press `Connect` in the app.
3. If the reader joined the same Wi-Fi network, the app discovers it automatically.
4. Otherwise, iOS offers the reader's `RSVP-Nano-xxxxxx` direct network.

The firmware shows either the shared Wi-Fi network or the direct network, plus the active local
address, while Companion Sync is open.

## Current Capabilities

- List, upload, and delete books/articles on the reader.
- Read and save reader settings.
- Read, save, and clear reader Wi-Fi settings.
- Set a book's saved resume location when indexed metadata is available.
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
