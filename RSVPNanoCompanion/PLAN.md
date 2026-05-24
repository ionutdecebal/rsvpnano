# RSVPNanoCompanion Plan

## Goal

Ship native iOS and Android companion apps backed by one Kotlin Multiplatform core.

- Shared Kotlin owns models, API access, conversion, persistence contracts, article/RSS workflows, settings rules, and device orchestration.
- SwiftUI and Compose own platform UI, permissions, and presentation state.
- Platform code adapts native storage, networking, and UI events into shared services.

## Current State

- [x] Shared Kotlin Multiplatform module builds for Android and is wired for iOS XCFramework CI.
- [x] Shared models are the source of truth for books, settings, drafts, RSS feeds, app settings, remembered Nano identity, and device snapshots.
- [x] Shared Ktor client owns RSVP Nano HTTP API behavior.
- [x] Shared `NanoCompanionController` owns connect/refresh/sync/delete/settings/RSS workflows.
- [x] Shared import preparation centralizes text/link draft normalization.
- [x] Shared settings helpers centralize Nano setting IDs, ranges, snapping, clamping, and appearance-mode logic.
- [x] Android app uses shared storage, device sync, uploads, delete, settings, Wi-Fi, RSS, saved article workflows, and share intents.
- [x] iOS app and share extension use shared converters/controller/wiring for app and share workflows.
- [x] Platform UI files are split into focused Compose and SwiftUI files.
- [x] Android app settings storage uses Android DataStore behind a shared `AppSettingsStore` contract.
- [x] iOS app settings storage uses shared JSON settings logic over app-group file storage. It does not depend on AndroidX DataStore.
- [x] Device mutations verify reachability before writes.
- [x] Local Windows verification builds shared checks and Android debug APKs.

## Priority 0: Keep Builds Green

- [ ] Keep Android CI passing.
- [ ] Keep iOS CI passing.
- [ ] Fix Kotlin/Native issues immediately when iOS CI reports them.
- [x] Remove iOS-incompatible AndroidX DataStore dependencies from common/iOS source sets.
- [ ] Keep GitHub Actions on supported runtimes.
- [ ] Keep generated iOS XCFramework artifact paths accurate.
- [x] Keep local Android verification passing:

```bash
./gradlew :shared:check :androidApp:assembleDebug --no-daemon --no-configuration-cache
```

## Priority 1: Hardware Behavior

- [x] Android hardware smoke test against real RSVP Nano.
  - [x] Upload works.
  - [x] Delete while powered off fails safely.
  - [x] Connect/refresh/upload/delete/settings/RSS/article sync pass retested after connection hardening.
- [ ] iOS hardware smoke test against real RSVP Nano.
- [x] Android destructive operations are blocked or fail safely while disconnected.
- [ ] Confirm iOS destructive operations are blocked or fail safely while disconnected.
- [ ] Confirm powered-off/disconnected reader state clears promptly on both platforms.
- [ ] Confirm core operations on both platforms:
  - [x] List books/articles.
  - [x] Upload converted books.
  - [x] Delete books/articles.
  - [x] Read and save reader settings.
  - [x] Read, save, and clear reader home Wi-Fi settings.
  - [x] Add and sync RSS feeds.
  - [x] Save, fetch, and sync article drafts.
- [ ] Firmware follow-up: ensure Nano-side upload/delete/finalize paths clean stale `.failed`, `.tmp`, and `.converting` sidecars for books and articles.
- [ ] Manually test iOS sharing from Safari, Chrome, and reader/file apps.
- [x] Manually test Android sharing from Chrome, reader, and file apps.
- [ ] Decide binary/book file sharing UX for share sheets:
  - [ ] Save as local pending book.
  - [ ] Direct upload only.
  - [ ] Leave unsupported with clear copy.

## Priority 2: Connection UX

Current implemented route:

- [x] Primary route tells the user to open Companion Sync and join the RSVP-Nano Wi-Fi.
- [x] App checks `http://192.168.4.1` when returning/connecting.
- [x] If the default address fails, user can enter the IP/address shown on the reader.
- [x] Writes preflight reader reachability before mutating device state.
- [x] Pull-to-refresh is used where supported for content/settings refresh.
- [x] Explicit connection controls remain available in the global connection surface.

Remaining work:

- [ ] Improve network-state responsiveness after disconnect/power-off events.
- [ ] Keep Nano API availability distinct from phone internet availability in UI state.
- [ ] Avoid treating one transient API failure as proof that the phone left the Nano AP.
- [ ] Improve connection copy for casual users who only see the Nano AP name and `http://192.168.4.1`.
- [ ] Add better success/failure messaging for Wi-Fi save/clear and settings save.

Deferred work:

- [ ] Native Nano AP discovery/connection.
  - [ ] Android: evaluate app-requested network attachment for `RSVP-Nano-*` without regressing the stable manual route.
  - [ ] iOS: evaluate `NEHotspotConfiguration`, entitlement needs, prompts, and App Store constraints.
- [ ] Remembered Nano / auto-connect.
  - [ ] Implement only after native discovery is reliable.
  - [ ] Remembering should happen after a verified Nano API connection.
  - [ ] Settings should include `Forget remembered Nano` only when a Nano is remembered.
- [ ] Optional auto-disconnect.
  - [ ] Defer until remembered/auto-connect is reliable.

## Priority 3: Shared Core

- [x] Shared app settings storage contract keeps platform storage behind common APIs.
- [x] Android app settings backend uses DataStore in `androidMain`.
- [x] iOS app settings backend uses shared JSON store and iOS app-group file storage.
- [x] Shared settings schema centralizes option IDs, ranges, snapping, clamping, and appearance helpers.
- [x] Shared device snapshot summary centralizes library status calculations.
- [x] Shared `NanoBook.id` is used consistently across iOS and Android.
- [x] Android ViewModel business flow was reviewed and moved into shared services where practical.
- [x] iOS ViewModels were reviewed and moved into shared services where practical.
- [ ] Keep Swift `Models.swift` as presentation extensions/adapters only.
- [ ] Continue moving platform-side business decisions into shared services when real duplication appears.

## Priority 4: Conversion And Parity

- [x] Conversion contract is documented in `docs/conversion-spec.md`.
- [x] Shared converter supports `.rsvp`, `.epub`, `.txt`, `.md`, `.markdown`, `.html`, `.htm`, and `.xhtml`.
- [x] Shared EPUB conversion supports ZIP parsing through Korlibs compression.
- [x] EPUB2 NCX and EPUB3 nav TOC chapter labels are preferred when available.
- [x] Python SD-card converter follows the shared conversion spec.
- [x] Website converter core is split from UI for CLI/parity reuse.
- [x] Cross-runtime parity checks cover Kotlin, Python, and web text/HTML conversion.
- [x] Android/JVM tests cover representative EPUB and existing `.rsvp` paths.
- [ ] Add iOS EPUB parity coverage in macOS CI.
- [ ] Low priority: add Markdown-aware conversion.

## Priority 5: Tests

- [x] Mocked API tests cover endpoint paths, query parameters, upload/delete contract, and response decoding.
- [x] Article fetch tests cover URL validation, fetch size guards, and HTML formatting.
- [x] Shared settings update/helper tests cover normalization and appearance behavior.
- [x] Import preparation edge-case tests exist.
- [x] Pending upload sync partial-failure tests exist.
- [x] RSS merge/de-duplication tests exist.
- [x] Parity fixtures live under `testdata/conversion`.
- [x] CI uploads parity diffs/artifacts when tests fail.
- [ ] Add tests for disconnected/powered-off state transitions once final UX behavior is implemented.
- [ ] Add tests around binary-file share workflow if that feature is added.

## Priority 6: UX And Polish

- [x] Bottom navigation uses Library and Settings as primary sections.
- [x] Library combines books, Nano articles, and local article drafts.
- [x] Book/article rows use type icons.
- [x] Transient feedback is structured by notice tone in shared code.
- [x] Android Compose UI has been split into focused files.
- [x] iOS SwiftUI screens have been split into focused files.
- [ ] Continue native polish pass on both platforms:
  - [ ] Text and strings.
  - [ ] Status colors.
  - [ ] Empty/loading/error states.
  - [ ] Device operation confirmations.
  - [ ] Phone-sized layout.
  - [ ] Accessibility labels and dynamic type/text scaling.
  - [ ] Color contrast.

## Priority 7: Documentation And Developer Experience

- [x] Contributor-facing Android README uses standard Gradle/ADB/Android Studio commands.
- [x] Personal `.local` tooling stays ignored and documented only inside `.local`.
- [x] Local bridge script was removed because direct Nano AP connection works.
- [x] Android README covers build, install, share-target, and hardware testing.
- [x] iOS README covers current shared framework integration and CI expectations.
- [ ] Document supported import/share types:
  - [x] Text.
  - [x] URLs.
  - [x] Text files.
  - [ ] EPUB/books after the intended UX is decided.
- [ ] Document RSVP Nano connection limitations:
  - [x] Firmware exposes AP details and `http://192.168.4.1`.
  - [x] Primary UX assumes the user joins the Nano AP when needed.
  - [ ] Document current manual connection flow and deferred native discovery/remembered Nano work.
- [ ] Document CI artifacts:
  - [ ] Android APK output.
  - [x] iOS XCFramework output.
- [ ] Add release checklist:
  - [ ] Android debug/release build.
  - [ ] iOS app build.
  - [ ] iOS share extension build.
  - [ ] Hardware smoke test.
  - [ ] Share-flow smoke test.
  - [ ] Basic accessibility pass.

## Definition Of Done

- [ ] Android CI passes.
- [ ] iOS CI passes.
- [ ] Shared parity tests cover representative text, HTML, EPUB, and existing `.rsvp` paths.
- [ ] iOS app builds and runs against real hardware.
- [x] Android app builds and runs against real hardware.
- [ ] Both apps can connect, refresh, upload, delete, sync articles, sync RSS, and update settings.
- [ ] Both share flows save drafts correctly.
- [ ] Disconnect/power-off behavior prevents stale destructive actions.
- [ ] Remaining platform code is UI/presentation/adaptation only.
- [ ] User-facing UX is polished enough for non-technical users.
