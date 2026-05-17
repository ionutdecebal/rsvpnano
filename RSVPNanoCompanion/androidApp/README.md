# RSVP Nano Android Companion

Native Android shell for the Kotlin Multiplatform companion migration.

The app keeps Android UI in Compose and calls the `:shared` module for persistence, converters,
RSS feed storage, and sync orchestration.

## Build

Install Android Studio or a JDK 17 + Android SDK environment, then run from the repository root:

```bash
bash ./gradlew :shared:check :androidApp:assembleDebug
```

The debug APK is written under:

```text
RSVPNanoCompanion/androidApp/build/outputs/apk/debug/
```

## Run

1. Open the repository root in Android Studio.
2. Select the `androidApp` run configuration.
3. Run on an emulator or Android device.

The current shell loads local shared storage for saved articles and RSS feeds, connects to the
reader through the shared device sync service, lists/deletes device books, creates/deletes saved
article drafts, edits saved article drafts, syncs ready text article drafts, fetches URL-only
article drafts through shared article formatting, and can add/sync RSS feeds.

Device API sync uses the shared `NanoClient`/sync interfaces and should stay thin in the Compose
layer. The Android entry point creates the Ktor-backed device client through
`createAndroidDeviceSyncService()`, while URL fetches use `createAndroidArticleFetchClient()`.
The app permits cleartext HTTP because the reader exposes its companion API at `http://192.168.4.1`
while in Companion sync mode.
