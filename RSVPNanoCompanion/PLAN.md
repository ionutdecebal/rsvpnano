# RSVPNanoCompanion Migration Plan

## Objective

Move the companion apps to a shared Kotlin Multiplatform core while keeping native platform UI:

- Shared Kotlin owns models, parsing, RSVP conversion, API access, persistence contracts, article/RSS workflows, and device orchestration.
- iOS stays SwiftUI, including a Swift share extension, but calls shared Kotlin for business logic.
- Android stays Jetpack Compose and calls the same shared Kotlin workflows.
- Platform code should be thin presentation/adaptation code, not a second implementation of business rules.

## Current Status

- [x] `shared` Kotlin Multiplatform module exists with Android and iOS targets.
- [x] Shared models are the source of truth for device/book/settings/draft/RSS data.
- [x] Shared Ktor API client handles RSVP Nano HTTP calls.
- [x] Shared RSVP text/file conversion owns the main conversion path.
- [x] Shared EPUB conversion exists through platform ZIP adapters.
- [x] Shared article formatting/fetching exists.
- [x] Shared pending-upload persistence exists with platform implementations.
- [x] Shared RSS feed persistence exists.
- [x] Shared `NanoCompanionController` owns app-level workflows.
- [x] Old shared facade pattern has been removed.
- [x] Android app shell uses shared storage, device sync, upload/list/delete, settings, Wi-Fi, RSS, and saved-article workflows.
- [x] Android share target accepts shared text, URLs, and text-file streams into shared draft storage.
- [x] iOS app uses shared converters/controller/wiring for app workflows.
- [x] iOS share extension calls shared logic for saved article/share flows.
- [x] iOS `ContentView` has been split into focused page/view files for library, articles, settings, help, and article editing.
- [x] Device mutations verify reachability before writes, so stale connected UI should clear when the Nano disconnects.
- [x] Android CI and iOS CI exist and build the relevant shared/app targets.
- [x] Local Windows verification covers shared Android compilation, shared tests/lint, and Android debug APK assembly.

## Priority 0: Keep The Build Green

These are blocking tasks. Do them before feature work when they fail.

- [ ] Keep Android CI passing.
- [ ] Keep iOS CI passing.
- [ ] Fix Kotlin/Native compatibility issues immediately when iOS CI reports them.
- [ ] Keep GitHub Actions current enough to avoid Node runtime deprecation failures.
- [ ] Keep Gradle wrapper executable in CI.
- [ ] Keep generated iOS XCFramework artifact upload paths accurate.
- [ ] Keep local Android verification passing:

```powershell
powershell -ExecutionPolicy Bypass -File .local\run_local_gradle.ps1 :shared:check :androidApp:assembleDebug --no-daemon --no-configuration-cache
```

## Priority 1: Functional Completion

These decide whether the apps are actually usable end-to-end.

- [ ] Manually smoke test iOS app against real RSVP Nano hardware.
- [ ] Manually smoke test Android app against real RSVP Nano hardware.
- [ ] Verify the primary connection UX on both platforms:
  - [x] Primary route tells the user to open Companion Sync on the reader and join the RSVP-Nano Wi-Fi.
  - [x] App checks `http://192.168.4.1` automatically when returning/connecting.
  - [x] If default address fails, user can enter the IP/address shown on the reader.
  - [ ] Confirm disconnected/powered-off reader updates UI promptly on both platforms.
- [ ] Verify core device operations on both platforms:
  - [x] List books/articles from the reader.
  - [x] Upload converted books.
  - [x] Delete books/articles from the reader.
  - [x] Read reader settings.
  - [x] Save reader settings.
  - [x] Read Wi-Fi settings.
  - [x] Save Wi-Fi settings.
  - [x] Clear Wi-Fi settings.
  - [x] Add RSS feeds locally.
  - [x] Sync RSS feeds to the reader.
  - [x] Save text/article drafts locally.
  - [x] Fetch URL-only article drafts.
  - [x] Sync saved articles to the reader.
  - [ ] Confirm all operations behave correctly after the Nano disconnects mid-session.
- [ ] Verify platform share flows:
  - [x] iOS share extension saves incoming URLs/text into shared draft storage.
  - [x] Android share target saves incoming URLs/text/text-file streams into shared draft storage.
  - [ ] Manually test iOS sharing from Safari/Chrome/reader apps.
  - [ ] Manually test Android sharing from Chrome/reader/file apps.
  - [ ] Decide whether binary file sharing should create a local pending book workflow, direct upload only, or remain unsupported.

## Priority 2: Shared-Core Hardening

These reduce the chance of app divergence and regression.

- [x] Shared `NanoCompanionController` centralizes repeated connect/refresh/sync/delete/settings/RSS flows.
- [x] Shared `ImportPreparation` centralizes text/link draft normalization.
- [x] Shared settings update helpers keep settings immutable while remaining usable from Swift.
- [x] Shared `NanoBook.id` is used consistently across iOS and Android.
- [ ] Move any remaining platform-side business decisions into shared services where practical.
- [ ] Review Android `CompanionViewModel` for logic that belongs in shared controller/service methods.
- [ ] Review iOS `NanoViewModel` for logic that belongs in shared controller/service methods.
- [ ] Split `NanoViewModel.swift` further if Swift-side orchestration remains large:
  - [ ] Connection/device state.
  - [ ] Settings/Wi-Fi/RSS.
  - [ ] Drafts/articles/share inbox.
  - [ ] Swift/Kotlin conversion helpers.
- [ ] Keep Swift `Models.swift` as presentation extensions/adapters only.
- [ ] Delete any remaining Swift business-logic files that are not compiled or no longer needed after iOS CI proves shared replacements.

## Priority 3: Test Coverage And Parity

The goal is confidence that Kotlin output matches the legacy behavior and remains stable.

- [x] Shared mocked API tests cover endpoint paths, query parameters, upload/delete contract, and response decoding.
- [x] Shared article fetch tests cover URL validation, fetch size guards, and HTML-to-readable formatting.
- [x] Android/JVM parity test verifies existing `.rsvp` demo pass-through byte-for-byte.
- [ ] Add EPUB-to-RSVP golden vectors.
- [ ] Add text-to-RSVP golden vectors.
- [ ] Add HTML/article formatting fixtures for common web pages.
- [x] Add tests for shared settings update helpers.
- [x] Add tests for shared import preparation edge cases:
  - [x] Empty titles.
  - [x] URL title/host cleanup.
  - [x] Text title inference.
  - [x] Source URL handling.
- [ ] Add tests for pending upload sync behavior after partial failures.
- [x] Add tests for RSS merge/de-duplication behavior.
- [ ] Store parity fixtures in a deterministic repo path, preferably under `shared/src/commonTest/resources` or `docs/test-vectors`.
- [ ] Have CI upload parity diffs/artifacts when tests fail.

## Priority 4: Platform UX And Polish

This is the product-quality phase after core workflows are stable.

- [ ] Define a shared product-level design direction:
  - [ ] Navigation model.
  - [ ] Information hierarchy.
  - [ ] Connection states.
  - [ ] Empty/loading/error states.
  - [ ] Device operation confirmations.
  - [ ] Saved article workflow.
  - [ ] RSS workflow.
  - [ ] Settings workflow.
- [ ] Redesign iOS SwiftUI screens with native polish.
- [ ] Redesign Android Compose screens with native polish.
- [ ] Improve connection UX copy for casual users who only see the Nano's AP name and `http://192.168.4.1`.
- [ ] Add clear offline/disconnected states that prevent destructive actions.
- [ ] Add automatic connection testing after returning from Wi-Fi settings.
- [ ] Add better success/failure messaging for Wi-Fi save/clear and settings save.
- [ ] Add accessibility pass:
  - [ ] Dynamic type/text scaling.
  - [ ] VoiceOver/TalkBack labels.
  - [ ] Touch target sizes.
  - [ ] Color contrast.
- [ ] Add phone-sized layout pass on both platforms.

## Priority 5: Local Developer Experience

Local tooling is intentionally developer-specific and should stay in `.local`, not project-level tools intended for maintainers.

- [x] Local Java/Gradle/Android SDK setup exists under `.local`.
- [x] `.local` README documents local setup.
- [x] Local Android debug APK install script exists.
- [x] Local Android emulator/device testing path exists.
- [x] Local bridge script was removed because connecting to the Nano AP directly works.
- [ ] Keep `.local` files out of shared maintainer workflows unless intentionally documented.
- [ ] Document common Android device testing commands:
  - [ ] USB install.
  - [ ] Wireless debugging pair/connect/install.
  - [ ] Launch app after install.
- [ ] Document that iOS verification is expected through CI/macOS, not Docker.

## Priority 6: Documentation And Release Readiness

- [ ] Update iOS README with current shared framework integration and CI expectations.
- [ ] Update Android README with build/install/share-target/testing instructions.
- [ ] Document supported import/share types:
  - [ ] Text.
  - [ ] URLs.
  - [ ] Text files.
  - [ ] EPUB/books, once the intended UX is decided.
- [ ] Document RSVP Nano connection limitations:
  - [ ] Firmware currently exposes AP details and `http://192.168.4.1`.
  - [ ] App cannot change firmware UI.
  - [ ] Primary UX assumes user joins the Nano AP when needed.
- [ ] Document CI artifacts:
  - [ ] Android APK/AAR outputs.
  - [ ] iOS XCFramework output.
- [ ] Add release checklist:
  - [ ] Android debug/release build.
  - [ ] iOS app build.
  - [ ] iOS share extension build.
  - [ ] Hardware smoke test.
  - [ ] Share-flow smoke test.
  - [ ] Basic accessibility pass.

## Deferred Or Explicitly Not Planned Yet

- [ ] LAN discovery for the Nano on home Wi-Fi is deferred. Without firmware support such as mDNS, stable hostnames, or an advertised API endpoint, scanning is likely unreliable and confusing.
- [ ] Docker for iOS is not useful for real local iOS app testing because Xcode/macOS is still required.
- [ ] Firmware changes are out of scope because we do not currently have permission to modify the Nano firmware.
- [ ] Binary file sharing into local pending drafts is not implemented because the current pending-upload model stores article text, not prebuilt `.rsvp` book bytes.

## Definition Of Done

The migration should be considered complete only when:

- [ ] Android CI passes.
- [ ] iOS CI passes.
- [ ] Shared parity tests cover representative text, HTML, EPUB, and existing `.rsvp` paths.
- [ ] iOS app builds and runs against real hardware.
- [ ] Android app builds and runs against real hardware.
- [ ] Both apps can connect, refresh, upload, delete, sync articles, sync RSS, and update settings.
- [ ] Both share flows save drafts correctly.
- [ ] Disconnect/power-off behavior prevents stale destructive actions.
- [ ] Remaining platform code is UI/presentation/adaptation only.
- [ ] User-facing UX is polished enough for non-technical users.
