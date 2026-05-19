# RSVP Nano Android Companion

Native Android companion app for RSVP Nano.

The Android UI is Jetpack Compose. Business logic comes from the `:shared` Kotlin Multiplatform
module: persistence, converters, RSS feed storage, article drafts, device API access, and sync
orchestration should stay shared.

## Build

Install Android Studio or a JDK 17 + Android SDK environment, then run from the repository root:

```bash
bash ./gradlew :shared:check :androidApp:assembleDebug
```

The debug APK is written under:

```text
RSVPNanoCompanion/androidApp/build/outputs/apk/debug/
```

The local Windows helper can build with the sandboxed `.local` toolchain:

```powershell
powershell -ExecutionPolicy Bypass -File .local\run_local_gradle.ps1 :shared:check :androidApp:assembleDebug --no-daemon --no-configuration-cache
```

## Run

1. Open the repository root in Android Studio.
2. Select the `androidApp` run configuration.
3. Run on an emulator or Android device.

Or install the current debug APK with the local helper after building:

```powershell
powershell -ExecutionPolicy Bypass -File .local\install_android_debug_apk.ps1 -Launch
```

Build, install, and launch in one command:

```powershell
powershell -ExecutionPolicy Bypass -File .local\install_android_debug_apk.ps1 -Build -Launch
```

If multiple Android devices/emulators are connected, pass the ADB serial:

```powershell
.local\android-sdk\platform-tools\adb.exe devices
powershell -ExecutionPolicy Bypass -File .local\install_android_debug_apk.ps1 -DeviceSerial SERIAL_FROM_ADB -Launch
```

## Wireless Device Install

ADB-over-Wi-Fi works, but Android's Wireless debugging flow usually requires pairing first from
Developer options.

```powershell
.local\android-sdk\platform-tools\adb.exe pair PHONE_LAN_IP:PAIRING_PORT
.local\android-sdk\platform-tools\adb.exe connect PHONE_LAN_IP:DEBUGGING_PORT
powershell -ExecutionPolicy Bypass -File .local\install_android_debug_apk.ps1 -DeviceSerial PHONE_LAN_IP:DEBUGGING_PORT -Launch
```

If the phone was first connected by USB and TCP mode is enabled:

```powershell
.local\android-sdk\platform-tools\adb.exe -s SERIAL_FROM_ADB tcpip 5555
powershell -ExecutionPolicy Bypass -File .local\install_android_debug_apk.ps1 -RemoteHost PHONE_LAN_IP -Launch
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
