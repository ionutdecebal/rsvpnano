# shared (Kotlin Multiplatform)

This module contains shared companion business logic for RSVPNanoCompanion. Document conversion
lives in `:conversionCore` and is consumed from this module.

Quick start:

- Run Android checks with `bash ./gradlew checkAndroid`.
- Run iOS checks with `bash ./gradlew checkIos`.
- Run web converter checks with `bash ./gradlew checkWeb`.
- Build the iOS XCFramework with `bash RSVPNanoCompanion/tools/build_shared_xcframework.sh`.
- Add the produced iOS framework to Xcode or add the module to your Android project.

Design goals:
- Keep platform-specific code minimal by using interfaces.
- Centralize companion workflow, API, persistence, and serialization logic in `commonMain`.
- Treat shared JSON stores as the only persistence contract for iOS and Android.
- Create platform Ktor clients from shared wiring (`createAndroidNanoClient`,
  `createIosNanoClient`) so UI code does not duplicate HTTP behavior.
- Keep converter behavior in `:conversionCore`; shared tests should cover companion workflows that
  consume converter APIs.
