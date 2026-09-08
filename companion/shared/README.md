# shared (Kotlin Multiplatform)

This module contains shared companion business logic for RSVPNanoCompanion. Document conversion
lives in `:conversionCore` and is consumed from this module.

Quick start:

- Run Android checks with `bash ./gradlew checkAndroid`.
- Run iOS checks with `bash ./gradlew checkIos`.
- Run the Compose/Wasm browser checks and stage the Pages site with `bash ./gradlew checkWeb`.
- Build the iOS app in Xcode. Its direct-integration phase builds the matching Kotlin framework.
- Open the Xcode project or add the module to your Android project.

Web JavaScript dependencies:

- Declare npm dependencies in the consuming module's Gradle dependencies and import them
  through Kotlin `@JsModule` or a bundled JavaScript module. Do not load executable
  dependencies from a CDN at runtime.
- Keep `kotlin-js-store/wasm/yarn.lock` in version control. After an intentional npm
  dependency change, run `bash ./gradlew kotlinWasmUpgradeYarnLock` and review the lockfile diff.
  Normal builds reject missing or out-of-date locks.
- `checkWeb` compiles the production bundle and runs browser tests, including loading the
  real firmware installer up to the serial-port picker. Missing exports fail Webpack;
  the installer check uses local firmware and does not access hardware.

Design goals:
- Keep platform-specific code minimal by using interfaces.
- Centralize companion workflow, API, persistence, and serialization logic in `commonMain`.
- Treat shared JSON stores as the only persistence contract for iOS and Android.
- Create platform presenters from `createAndroidCompanionPresenter` and
  `createIosCompanionPresenter` so UI code does not duplicate HTTP or storage wiring.
- Keep converter behavior in `:conversionCore`; shared tests should cover companion workflows that
  consume converter APIs.

Book operations use the opaque `NanoBook.id` returned by the device API. The display name remains
separate from identity, and progress edits include source size, source fingerprint, word count, and
word index so stale book indexes are rejected by the firmware.
