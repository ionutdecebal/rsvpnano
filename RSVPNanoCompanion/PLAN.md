## Plan: Kotlin Multiplatform Migration for RSVPNanoCompanion

TL;DR - Move reusable business logic (parsers, converters, models, sync, API client) into a Kotlin Multiplatform `shared` module and keep platform UI native (SwiftUI on iOS, Compose on Android). Keep the iOS Share Extension in Swift but call into the shared module for parsing/formatting. Use Ktor, kotlinx.serialization, coroutines, and expect/actual for platform-specific storage.
**Minimize Duplication (Principles & Actions)**
- **Single Source of Truth:** Move all parsing, conversion, binary serialization, and API DTOs into `commonMain` so platform UIs call the same shared code.
- **Clear Abstractions:** Define small interfaces in `commonMain` (e.g., `PendingUploadStore`, `FileProvider`, `HttpClient`) and implement platform specifics in `iosMain`/`androidMain` via `expect/actual` to avoid duplicated logic.
- **Thin UI Adapters:** Keep a thin adapter layer (`NanoViewModel` in Swift, `ViewModel` in Android) that maps shared domain models to UI state; adapters contain only mapping/presentation logic, not business rules.
- **Shared Utilities:** Centralize utilities (encoding detection, regex patterns, HTML cleanup rules) in `commonMain` and reuse them across converters to prevent divergent behaviors.
- **Reuse Tests:** Implement shared `commonTest` unit tests for converters and writers and run them on both platforms to detect regressions and ensure parity.
- **Binary Parity Gate:** Create byte-for-byte comparison tests for RSVP output; CI must run these to prevent accidental divergence.
- **Documentation-Driven Contract:** Document public shared APIs and data formats; use these docs to keep platform adapters minimal and avoid re-implementations.
- **Gradual Migration:** Port library code first and remove Swift business-logic fallbacks once Kotlin implementations pass parity tests; this avoids duplicating production code during transition.



**Steps**
1. Create KMP module: `shared` (Gradle Kotlin Multiplatform). Define `commonMain`, `iosMain`, `androidMain`.
2. Implement models in `commonMain` with `kotlinx.serialization` (map from `Models.swift` to `RSVPNanoCompanion/shared/src/commonMain/.../NanoModels.kt`).
3. Port text & EPUB logic to `commonMain`:
   - Port `RsvpConverter.swift` → `RsvpConverter.kt` and `RsvpWriter.kt` (exact binary parity tests required).
   - Port `EpubConverter.swift` → `EpubConverter.kt` (use `java.util.zip` / platform zip libs).
   - Port `ArticleFormatter.swift` → `ArticleFormatter.kt`.
4. Implement `NanoClient` in `commonMain` using Ktor (map endpoints from `NanoClient.swift`).
5. Define persistence interfaces in `commonMain` (`PendingUploadStore`, `RssFeedStore`) and provide `expect/actual` implementations:
   - `iosMain` uses FileManager + App Group and UserDefaults.
   - `androidMain` uses `Context` + SharedPreferences / Room.
6. Implement `SyncManager` in `commonMain` replicating logic from `NanoViewModel` (RSS merge, pending uploads orchestration). Keep UI state management on platform side. Expose suspend APIs for platform callers.
7. Integrate with iOS:
   - Build KMP iOS framework (XCFramework) and add to Xcode project.
   - Replace calls in `ContentView.swift`/`NanoViewModel` to call shared `SyncManager`/`NanoClient` via Swift coroutines bridge (use `@objc`/Kotlin/Native generated interop). Keep Swift-only models thin and keep persistence in shared.
   - Keep Share extension Swift-only but call shared converters for formatting.
8. Implement Android app shell (Compose) that mirrors SwiftUI flows and calls `shared` APIs.
   - Current Android shell uses shared storage, shared Ktor device sync, device book listing/deletion, saved article create/edit/delete, ready text article sync, URL-only article fetching, and RSS add/sync.
   - Remaining Android gap: manual device smoke testing on hardware/emulator and UI polish.
9. Testing and parity verification:
   - Add `commonTest` unit tests for converters and models.
   - Create byte-for-byte tests comparing RSVP output from Swift implementation vs Kotlin implementation using sample inputs from `docs/demo-books`.
   - Current parity gate: `RsvpDemoBookParityAndroidTest` verifies `docs/demo-books/european-letter-demo.rsvp` passes through byte-for-byte on Android/JVM tests.
   - Mocked API smoke tests cover shared Ktor endpoint paths, query parameters, upload/delete contract, and response decoding.
   - Mocked article fetch tests cover shared URL validation, fetch size guards, and HTML-to-readable-article formatting.
10. Documentation & CI:
  - Document build steps, Xcode integration, and Android Gradle setup in `RSVPNanoCompanion/ios/README.md` and `RSVPNanoCompanion/android/README.md`.
   - Add CI jobs to build `shared` for Android and iOS and run `commonTest`.

**Relevant files**
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/ContentView.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/ContentView.swift) — main UI + `NanoViewModel` to adapt to shared `SyncManager`.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/Models.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/Models.swift) — source of DTOs to port.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/NanoClient.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/NanoClient.swift) — REST endpoints to implement with Ktor.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/PendingUploadStore.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/PendingUploadStore.swift) — thin Swift UI model/app-group constants; storage is owned by `shared`.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/RsvpConverter.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/RsvpConverter.swift) — complex parser & writer to port to `shared`.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/EpubConverter.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/EpubConverter.swift) — EPUB parsing to port.
- [RSVPNanoCompanion/ios/RSVPNanoCompanion/ArticleFormatter.swift](RSVPNanoCompanion/ios/RSVPNanoCompanion/ArticleFormatter.swift) — HTML extraction logic to port.
- [RSVPNanoCompanion/ios/RSVPNanoShareExtension/ShareViewController.swift](RSVPNanoCompanion/ios/RSVPNanoShareExtension/ShareViewController.swift) — keep Swift, call into `shared` converters.

**Verification**
1. Unit tests in `RSVPNanoCompanion/shared/commonTest` for `RsvpConverter`, `EpubConverter`, and `ArticleFormatter` matching Swift outputs.
2. Binary parity: Android/JVM unit tests currently verify byte-for-byte pass-through for `RSVPNanoCompanion/docs/demo-books/european-letter-demo.rsvp`; add EPUB-to-RSVP golden vectors as sample EPUBs become available.
3. Manual app smoke test on iOS: integrate `shared` framework, run app, upload book to device using existing pairing flow.
4. Android smoke test: simple Compose screens call `shared` SyncManager to list `NanoBook` DTOs (mocked API), and create an RSVP from sample EPUB.
5. CI: ensure Gradle builds for `android`, Kotlin `commonTest` passes, and `ios` XCFramework builds.

**Decisions & Assumptions**


**Scaffold: `shared` module (detailed)**

TL;DR: Create a small, maintainable KMP `shared` module that contains models, converters, API client interfaces, and persistence interfaces. Keep platform specifics out of `commonMain` by using small interfaces + dependency injection.

Files & Layout (create under `RSVPNanoCompanion/shared/`)
- `RSVPNanoCompanion/shared/build.gradle.kts` — KMP build file with Android + iOS targets and `kotlinx.serialization`, `ktor-client`, and `coroutines` dependencies.
- `RSVPNanoCompanion/shared/settings.gradle.kts` — includeProject config if needed.
- `RSVPNanoCompanion/shared/src/commonMain/kotlin/com/rsvpnano/`:
  - `models/NanoModels.kt` — `@Serializable` DTOs (example: `NanoBook`).
  - `persistence/PendingUploadStore.kt` — interface for persistence (suspend functions); implementations provided by platform.
  - `api/NanoClient.kt` — interface for API operations (suspend functions); implementation in common using Ktor or provided by DI.
  - `converters/RsvpConverter.kt` — placeholder for shared converter logic (later port).
- `RSVPNanoCompanion/shared/src/commonTest/kotlin/...` — test templates for parity/unit tests.
- `RSVPNanoCompanion/shared/src/iosMain/kotlin/...` — thin adapter that binds `FileManager`/AppGroup to `PendingUploadStore` and provides iOS-specific wiring.
- `RSVPNanoCompanion/shared/src/androidMain/kotlin/...` — `Context`-based implementation for `PendingUploadStore`.

Minimal `build.gradle.kts` snippet (conceptual)
- Apply plugins: `kotlin("multiplatform")`, `kotlin("plugin.serialization")`
- Targets: `android()`, `ios { binaries.framework { baseName = "shared" } }`
- Dependencies (commonMain): `implementation("io.ktor:ktor-client-core:2.x")`, `implementation("org.jetbrains.kotlinx:kotlinx-serialization-core:1.x")`, `implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.7.0")`

Example DTO (`NanoBook`) for `commonMain` (concept)
- `@Serializable data class NanoBook(val id: String? = null, val title: String, val author: String? = null, val progressPercent: Int? = null, val category: String? = null)`

Persistence Interface (common)
- `interface PendingUploadStore { suspend fun loadAll(): List<PendingUpload>; suspend fun saveAll(items: List<PendingUpload>) }
- Implementations: `IosPendingUploadStore` in `iosMain` (use FileManager + App Group), `AndroidPendingUploadStore` in `androidMain` (Context + files or SharedPreferences). Inject store into `SyncManager` at startup.

Why interface+DI (recommended): keeps `commonMain` platform-agnostic, avoids `expect/actual` boilerplate, and is simpler to test and maintain.

Example test template (commonTest)
- `RsvpParityTest` — load `RSVPNanoCompanion/docs/demo-books/sample.epub`, run `RsvpConverter.createRsvp(...)`, compute SHA256 and assert equals expected checksum stored in `test/resources`.

Verification for scaffold
1. `./gradlew :shared:assemble` builds the module for Android and produces an XCFramework for iOS (or use Gradle tasks to produce frameworks). 2. Add `shared` as a dependency to Android app module. 3. Integrate built framework into Xcode project and compile.

Next actionable option (pick one)
- Implement the `shared` Gradle scaffold and add `NanoBook` + `PendingUploadStore` interface and a tiny unit test template (I will generate files).  
- OR generate `commonTest` parity test vectors and harness only (no build files).

Recommendation: implement the scaffold first (Gradle + one DTO + interface + test template). This gives a minimal working KMP artifact to iterate on and avoids duplicated platform-specific code.

- Share extension remains Swift-only; only parsing/formatting logic is migrated.
- Binary RSVP format must be byte-identical; tests will gate merging.
- No closed-source dependencies were found; assume pure-Swift codebase.
- Use Ktor + kotlinx.serialization + coroutines as primary KMP stack.

**Further Considerations**
1. Do you want the iOS UI to call the shared `SyncManager` directly from Swift, or keep a thin `NanoViewModel` adapter on the Swift side that calls `shared` (recommended: keep adapter for minimal UI changes)?
2. Would you prefer `SQLDelight` for multi-platform persistence (more structure) or simple `expect/actual` file + SharedPreferences adapters (faster)?
3. Would you like CI to publish the built iOS XCFramework as an artifact for easier Xcode integration?



**CI/CD (Detailed)**

Objective: Ensure builds, tests, and parity checks run automatically on PRs; build artifacts (XCFramework, AAR) are produced and optionally published. Gate merges on unit + parity tests.

Workflows (GitHub Actions recommended):
- `ci-android.yml` (runs on `ubuntu-latest`):
  - Checkout, cache Gradle, run `./gradlew :shared:assemble` and `./gradlew :shared:check`.
  - Upload artifacts: `shared/build/outputs` (AAR/jars) if needed.
  - Steps:
    1. Setup JDK (E.g., `temurin` 17 or 21).
    2. Cache Gradle and `.gradle` dependencies.
    3. Run `./gradlew :shared:assemble :shared:connectedCheck` (if Android emulator tests added later).
    4. Run `./gradlew :shared:check --no-daemon` to run `commonTest`.
  - Use matrix if testing multiple Kotlin versions or Ktor variants.

- `ci-ios.yml` (runs on `macos-latest`):
  - Checkout, install CocoaPods if needed, setup JDK, run `./gradlew :shared:assemble` to build XCFramework, run `xcodebuild` to build the iOS app target (optional smoke test).
  - Run parity tests that require macOS (byte-for-byte RSVP parity):
    1. Run Kotlin `commonTest` on macOS via Gradle.
    2. Run Swift parity generator (a small Swift script or existing Swift command) to produce expected RSVP outputs OR run Kotlin converter and compare to stored golden files. The goal: run a parity harness that compares expected vs produced outputs.
  - Upload artifacts: `shared/build/libs` and the generated `.xcframework` as workflow artifacts.
  - Use macOS runners to sign and, optionally, publish the XCFramework to GitHub Packages or internal artifact storage.

- `ci-parity.yml` (optional, runs on `macos-latest` and triggered on schedule or PR):
  - Specific job to run byte-for-byte tests across a set of sample books under `docs/demo-books`.
  - Fail the job if any checksum differs.

Quality Gates and Policies:
- Require `ci-android` and `ci-ios` to pass before merging to `main`.
- Enforce branch protection rules: require passing checks, require PR reviews.
- Publish test artifacts and parity reports as job artifacts for debugging failing tests.

Artifacts & Releases:
- Build and upload `shared.xcframework` and `shared.aar` (or AAR/artifact) to GitHub Actions artifacts on successful main builds.
- Optionally publish the `shared` artifact to GitHub Packages (Maven repo) for Android consumption; use secrets for credentials.
- Optionally publish XCFramework to an internal binary repository or attach to GitHub Releases.

Monitoring and Notifications:
- Fail PRs loudly; post comments with failing test names and links to artifacts.
- Store parity test failure diffs as artifacts to speed debugging.

Implementation notes:
- Parity tests require deterministic inputs; store sample inputs and expected checksums in `RSVPNanoCompanion/shared/src/commonTest/resources` or a new `ci/test-vectors/` folder.
- Keep CI jobs minimal at first: build + commonTest on Ubuntu, build + parity on macOS. Expand later to run Android UI tests or iOS UI tests if needed.
- Cache Gradle and Ktor/Kotlin dependencies to speed repeated CI runs.

Security & Secrets:
- Use GitHub Secrets for publishing credentials and code-signing keys.
- Limit macOS runner usage via conditional jobs when possible; run parity tests on pull requests but schedule heavy parity checks nightly for large vectors.

CI Checklist (first iteration):
1. `ci-android.yml` builds Gradle `:shared` and runs `commonTest`.
2. `ci-ios.yml` builds `:shared` XCFramework and runs parity checks on a small sample.
3. Configure branch protection to require both checks.
4. Add artifact upload steps and store expected checksums in repo.



- Current constraint: iOS framework verification requires macOS/Xcode; local Windows verification covers shared Android compilation, shared unit tests, lint, and Android debug APK assembly.
