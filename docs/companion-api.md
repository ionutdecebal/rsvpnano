# Companion API v2

The reader exposes this API only while Companion Sync is open. There is no v1 or legacy-response compatibility.

`GET /api/v2/device` is the connection handshake. The companion loads the remaining resources on
demand for the active screen; they are not a bootstrap sequence and are not combined into a second
aggregate endpoint. A failure from one resource does not imply that the transport or device session
was lost.

## Responses

| Method | Path | Success response |
| --- | --- | --- |
| `GET` | `/api/v2/device` | `{ "ssid": "RSVP-Nano-XXXXXX", "firmwareVersion": "…", "otaAsset": "…" }` |
| `GET` | `/api/v2/library` | Bare array of library entries |
| `POST` | `/api/v2/library?category=…` | Created entry (`201` and `Location`) |
| `DELETE` | `/api/v2/library/{id}` | `204` |
| `PUT` | `/api/v2/library/{id}/position` | `204` |
| `PUT` | `/api/v2/library/{id}/language-fonts` | `204` |
| `GET` | `/api/v2/themes` | Bare array of `{ "id": "…", "name": "…" }` |
| `POST` | `/api/v2/themes` | Created theme (`201` and `Location`) |
| `DELETE` | `/api/v2/themes/{id}` | `204` |
| `GET` | `/api/v2/fonts` | Bare array of installed font summaries |
| `POST` | `/api/v2/fonts` | Created font (`201` and `Location`) |
| `DELETE` | `/api/v2/fonts/{id}` | `204` |
| `GET` | `/api/v2/locales` | Bare array of `{ "id": "…", "name": "…", "locale": "…" }` |
| `POST` | `/api/v2/locales` | Created locale (`201` and `Location`) |
| `DELETE` | `/api/v2/locales/{id}` | `204` |
| `PUT` | `/api/v2/appearance/theme` | `204` |
| `PUT` | `/api/v2/appearance/font` | `204` |
| `PUT` | `/api/v2/appearance/locale` | `204` |
| `GET` | `/api/v2/settings` | `{ "reading": …, "interface": …, "updates": … }` |
| `PATCH` | `/api/v2/settings/reading` | `204` |
| `PATCH` | `/api/v2/settings/display` | `204` |
| `PATCH` | `/api/v2/settings/updates` | `204` |
| `GET` | `/api/v2/network` | `{ "ssid": "…" }` |
| `PUT`, `DELETE` | `/api/v2/network` | `204` |
| `GET` | `/api/v2/feeds` | Bare array of feed URLs |
| `PUT` | `/api/v2/feeds` | `204` |
| `GET` | `/api/v2/focus-timers` | Bare array of timers |
| `PUT` | `/api/v2/focus-timers` | `204` |

All successful mutations return no body unless they create a resource. The companion already knows the submitted value and updates its local state after `204`; it does not need the device to echo it.

Collection responses are bare arrays. The collection name is already present in the URL, so wrapper objects such as `{ "items": [...] }` or `{ "locales": [...] }` are not sent.

The device sends only data the companion cannot derive locally:

- Library progress is only `wordIndex`; percentage, remaining words, current chapter, and reading time come from `wordCount`, `chapters`, and the current WPM.
- A library entry's article/book type comes from its path.
- Locale, theme, and downloadable-font details come from their published catalogs. Installed-resource responses contain only the identity and device-specific compatibility data needed by the app.
- Active theme, font, and locale IDs come from `/settings`; there are no duplicate appearance-selection reads.
- Wi-Fi state contains only the SSID. A stored-password flag is not useful to the companion.

Uploads use an `application/octet-stream` body. Library entries and themes take the URL-encoded `name` query parameter; fonts use the validated RFont4 header, and locale packs use their manifest.

## Errors

Errors use their HTTP status and a JSON body with a stable developer-facing code and message. Validation errors may also identify the affected field.

```json
{
  "code": "font_not_found",
  "message": "Font is not installed",
  "field": "id"
}
```

## Real-device contract test

The opt-in hardware test flashes firmware that boots directly into the production Companion API, discovers its URL over USB serial, and runs the Kotlin client against the ESP32:

```powershell
python tools/companion/run_device_api_test.py
```

The default run performs three complete read passes and reports request latency. Add `--write` to reapply current settings and appearance selections, round-trip feeds and focus timers, and upload then delete a disposable book. Network credentials are never changed.

Use `--no-upload --base-url http://DEVICE_IP` to rerun against API-test firmware already installed. The hardware test is skipped during ordinary Gradle checks unless `RSVPNANO_DEVICE_URL` is set.

The implementation is organized by domain under `src/companion`; the main app contains no benchmark or API-test branches.

### USB connection lifetime

USB companion sessions remain open until the host closes the port, the cable disconnects, or a transport error ends the session. There is no idle timeout and the browser sends no periodic keepalive probes. The browser asserts DTR and RTS when opening the serial port; firmware uses the native CDC connection state to release abandoned sessions.

Firmware advertises this behavior with `RSVPNANO/COMPANION/1 READY persistent\n`. The browser requires this capability and requests a firmware update for older devices with timed sessions. Existing clients can still send Ping frames and receive Pong responses.
