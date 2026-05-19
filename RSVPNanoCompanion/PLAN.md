# RSVPNanoCompanion Plan

## Goal

Ship native iOS and Android companion apps backed by one Kotlin Multiplatform core.

- Shared Kotlin owns models, API access, conversion, persistence, article/RSS workflows, and device orchestration.
- SwiftUI and Compose own platform UI, permissions, and presentation state.
- Platform code should adapt UI events into shared services, not duplicate business rules.

## Status Snapshot

- [x] Shared Kotlin Multiplatform module builds for Android and iOS CI targets.
- [x] Shared models are the source of truth for books, settings, drafts, RSS feeds, and device snapshots.
- [x] Shared Ktor client owns RSVP Nano HTTP API behavior.
- [x] Shared `NanoCompanionController` owns connect/refresh/sync/delete/settings/RSS workflows.
- [x] Shared facade pattern has been removed.
- [x] Android app uses shared storage, device sync, uploads, delete, settings, Wi-Fi, RSS, and saved article workflows.
- [x] Android share target accepts URLs, text, and text-file streams into shared draft storage.
- [x] iOS app uses shared converters/controller/wiring for app workflows.
- [x] iOS share extension uses shared logic for saved article/share flows.
- [x] iOS views are split into focused page/view-model files.
- [x] Device mutations verify reachability before writes.
- [x] Local Windows verification builds shared checks and Android debug APKs.

## Priority 0: Keep CI And Local Builds Green

- [ ] Keep Android CI passing.
- [ ] Keep iOS CI passing.
- [ ] Fix Kotlin/Native issues immediately when iOS CI reports them.
- [ ] Keep GitHub Actions on supported Node runtimes.
- [ ] Keep Gradle wrapper executable in CI.
- [ ] Keep generated iOS XCFramework artifact paths accurate.
- [ ] Keep local Android verification passing:

```bash
./gradlew :shared:check :androidApp:assembleDebug --no-daemon --no-configuration-cache
```

## Priority 1: Hardware And App Behavior

- [ ] Manually smoke test iOS against real RSVP Nano hardware.
- [ ] Manually smoke test Android against real RSVP Nano hardware.
  - [x] Uploading to the Nano from a physical Android phone works.
  - [x] Deleting while the Nano is powered off fails safely with a connection error instead of silently succeeding.
  - [ ] Re-test full connect/refresh/upload/delete/settings/RSS/article sync pass after connection-delay UX is fixed.
- [ ] Confirm powered-off/disconnected reader state clears promptly on both platforms.
- [x] Confirm destructive operations are blocked or fail safely while disconnected on Android.
- [ ] Confirm destructive operations are blocked or fail safely while disconnected on iOS.
- [ ] Confirm core device operations on both platforms:
  - [x] List books/articles.
  - [x] Upload converted books.
  - [x] Delete books/articles.
  - [x] Read and save reader settings.
  - [x] Read, save, and clear Wi-Fi settings.
  - [x] Add RSS feeds locally.
  - [x] Sync RSS feeds to the reader.
  - [x] Save local text/article drafts.
  - [x] Fetch URL-only article drafts.
  - [x] Sync saved articles to the reader.
- [ ] Manually test iOS sharing from Safari, Chrome, and reader/file apps.
- [ ] Manually test Android sharing from Chrome, reader, and file apps.
- [ ] Decide the UX for binary file sharing:
  - [ ] Save as a local pending book.
  - [ ] Direct upload only.
  - [ ] Leave unsupported with clear copy.

## Priority 2: Connection UX

- [x] Primary route tells the user to open Companion Sync and join the RSVP-Nano Wi-Fi.
- [x] App checks `http://192.168.4.1` automatically when returning/connecting.
- [x] If the default address fails, user can enter the IP/address shown on the reader.
- [ ] Add a short delay/retry window after returning from Wi-Fi settings or selecting the Nano AP before declaring connection failure.
- [ ] Improve connection copy for casual users who only see the Nano AP name and `http://192.168.4.1`.
- [ ] Add clearer offline/disconnected states.
- [x] Implement background article fetching: update `shared` module to allow automated, non-interactive fetches, enabling the app to pre-fetch content while the device has internet and eliminating the "fetch" button in UI.
- [ ] Add better success/failure messaging for Wi-Fi save/clear and settings save.

## Priority 3: Shared-Core Hardening

- [x] Shared import preparation centralizes text/link draft normalization.
- [x] Shared address/date helpers centralize platform-neutral formatting.
- [x] Shared device snapshot summary centralizes library status calculations.
- [x] Shared settings update helpers keep settings immutable and Swift-friendly.
- [x] Shared `NanoBook.id` is used consistently across iOS and Android.
- [x] Android ViewModel business flow was reviewed and moved into shared services where practical.
- [x] iOS ViewModels were reviewed and moved into shared services where practical.
- [ ] Keep Swift `Models.swift` as presentation extensions/adapters only.
- [ ] Delete any remaining Swift business-logic files that are not compiled or no longer needed after iOS CI proves shared replacements.
- [ ] Continue moving platform-side business decisions into shared services when new duplication appears.

## Priority 4: Conversion And Parity

- [x] Conversion contract is documented in `docs/conversion-spec.md`.
- [x] Shared converter supports `.rsvp`, `.epub`, `.txt`, `.md`, `.markdown`, `.html`, `.htm`, and `.xhtml`.
- [x] Shared EPUB conversion supports ZIP parsing through Korlibs compression.
- [x] EPUB2 NCX and EPUB3 nav TOC chapter labels are preferred when available.
- [x] EPUB generated filename chapter fallback is only used when no usable TOC exists.
- [x] Dracula EPUB fixtures preserve TOC subtitles when present; some source TOC entries intentionally contain only `CHAPTER <roman numeral>`.
- [x] Python SD-card converter follows the shared conversion spec.
- [x] Website converter core is split from UI for CLI/parity reuse.
- [x] Cross-runtime parity checks cover Kotlin, Python, and web text/HTML conversion.
- [x] Android/JVM tests cover representative EPUB and existing `.rsvp` paths.
- [ ] Add iOS EPUB parity coverage in macOS CI.
- [ ] Low priority: add Markdown-aware conversion:
  - [ ] Headings become chapters.
  - [ ] Emphasis/link/list/blockquote syntax becomes readable text.
  - [ ] Code fences and tables have deterministic fallback behavior.
  - [ ] Kotlin, Python, and web share Markdown reference cases.

## Priority 5: Tests

- [x] Mocked API tests cover endpoint paths, query parameters, upload/delete contract, and response decoding.
- [x] Article fetch tests cover URL validation, fetch size guards, and HTML formatting.
- [x] Shared settings update helper tests exist.
- [x] Import preparation edge-case tests exist.
- [x] Pending upload sync partial-failure tests exist.
- [x] RSS merge/de-duplication tests exist.
- [x] Parity fixtures live under `testdata/conversion`.
- [x] CI uploads parity diffs/artifacts when tests fail.
- [ ] Add tests for disconnected/powered-off state transitions once the final UX behavior is implemented.
- [ ] Add tests around any binary-file share workflow if that feature is added.

## Priority 6: UX And Polish

- [ ] Define a product-level design direction:
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
- [ ] Accessibility pass:
  - [ ] Dynamic type/text scaling.
  - [ ] VoiceOver/TalkBack labels.
  - [ ] Touch target sizes.
  - [ ] Color contrast.
- [ ] Phone-sized layout pass on both platforms.

## Priority 7: Documentation And Developer Experience

- [x] Contributor-facing Android README uses standard Gradle/ADB/Android Studio commands.
- [x] Personal `.local` tooling stays ignored and documented only inside `.local`.
- [x] Local bridge script was removed because direct Nano AP connection works.
- [x] Keep `.local` files out of maintainer workflows.
- [x] Update Android README with build, install, share-target, and hardware testing instructions.
- [x] Update iOS README with current shared framework integration and CI expectations.
- [ ] Document supported import/share types:
  - [x] Text.
  - [x] URLs.
  - [x] Text files.
  - [ ] EPUB/books after the intended UX is decided.
- [ ] Document RSVP Nano connection limitations:
  - [x] Firmware exposes AP details and `http://192.168.4.1`.
  - [x] App cannot change firmware UI.
  - [x] Primary UX assumes the user joins the Nano AP when needed.
- [ ] Document CI artifacts:
  - [ ] Android APK/AAR outputs.
  - [x] iOS XCFramework output.
- [ ] Add release checklist:
  - [ ] Android debug/release build.
  - [ ] iOS app build.
  - [ ] iOS share extension build.
  - [ ] Hardware smoke test.
  - [ ] Share-flow smoke test.
  - [ ] Basic accessibility pass.

## Deferred

- [ ] LAN discovery is deferred until firmware exposes a reliable discovery mechanism such as mDNS, a stable hostname, or an advertised API endpoint.
- [ ] Docker is not useful for real iOS app testing because Xcode/macOS is still required.
- [ ] Firmware changes are out of scope unless permission to modify the Nano firmware changes.
- [ ] Firmware Companion sync screen bug: while Companion sync is open, swipe/hold can still enter reading, then the UI returns to the sync screen.
- [ ] Binary file sharing into local pending drafts is deferred until the product UX is defined.

## Definition Of Done

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
