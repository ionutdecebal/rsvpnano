# shared (Kotlin Multiplatform)

This module contains shared business logic for RSVPNanoCompanion.

Quick start (local development):

- From repository root, run `bash ./gradlew :shared:assemble` to build Android and iOS frameworks.
- Add the produced iOS framework to Xcode or add the module to your Android project.

Design goals:
- Keep platform-specific code minimal by using interfaces.
- Centralize parsing, encoding, and serialization logic in `commonMain`.
