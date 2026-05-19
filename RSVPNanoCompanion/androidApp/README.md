# RSVP Nano Android Companion

Native Android companion app for RSVP Nano.

The Android UI is Jetpack Compose. Business logic comes from the `:shared` Kotlin Multiplatform
module: persistence, converters, RSS feed storage, article drafts, device API access, and sync
orchestration should stay shared.

## Build

Install Android Studio or a JDK 17 + Android SDK environment, then run from the repository root:

```bash
./gradlew :shared:check :androidApp:assembleDebug
```

The debug APK is written under:

```text
RSVPNanoCompanion/androidApp/build/outputs/apk/debug/
```

## Run

1. Open the repository root in Android Studio.
2. Select the `androidApp` run configuration.
3. Run on an emulator or Android device.

Or install the debug APK with Android SDK platform tools:

```bash
adb install -r RSVPNanoCompanion/androidApp/build/outputs/apk/debug/androidApp-debug.apk
adb shell monkey -p com.rsvpnano.android -c android.intent.category.LAUNCHER 1
```

If multiple Android devices/emulators are connected, pass the ADB serial:

```bash
adb devices
adb -s SERIAL_FROM_ADB install -r RSVPNanoCompanion/androidApp/build/outputs/apk/debug/androidApp-debug.apk
adb -s SERIAL_FROM_ADB shell monkey -p com.rsvpnano.android -c android.intent.category.LAUNCHER 1
```

## Wireless Device Install

ADB-over-Wi-Fi works, but Android's Wireless debugging flow usually requires pairing first from
Developer options.

```bash
adb pair PHONE_LAN_IP:PAIRING_PORT
adb connect PHONE_LAN_IP:DEBUGGING_PORT
adb -s PHONE_LAN_IP:DEBUGGING_PORT install -r RSVPNanoCompanion/androidApp/build/outputs/apk/debug/androidApp-debug.apk
```

If the phone was first connected by USB and TCP mode is enabled:

```bash
adb -s SERIAL_FROM_ADB tcpip 5555
adb connect PHONE_LAN_IP:5555
adb -s PHONE_LAN_IP:5555 install -r RSVPNanoCompanion/androidApp/build/outputs/apk/debug/androidApp-debug.apk
```

## Connect To The Reader

1. Put the reader into `Companion sync`.
2. Join the `RSVP-Nano-xxxxxx` Wi-Fi network shown on the reader from Android Wi-Fi settings.
3. Return to the app.
4. The app checks `http://192.168.4.1` automatically.
5. If the default address is not reachable, enter the address shown on the reader.

The app permits cleartext HTTP because the reader exposes its companion API at
`http://192.168.4.1` while in Companion sync mode.

## Share Target

The Android share target accepts:

- URLs.
- Plain text.
- Text-like files: `.txt`, `.md`, `.markdown`, `.html`, `.htm`, `.xhtml`.

Shared URLs/text are saved as local article drafts through shared import preparation. Text-like
files are read and saved as local drafts. Open the app later, connect to the reader, then sync saved
articles.

## Current Capabilities

The app can list/upload/delete books, read and save device settings, read/save/clear Wi-Fi
settings, add local RSS feeds, sync RSS feeds, save article drafts, fetch URL-only article drafts,
and sync saved articles. Device API sync uses the shared `NanoClient`/controller services and should
stay thin in the Compose layer.
