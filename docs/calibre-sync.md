# Calibre Library Sync

Device-direct pull of `.rsvp` books from a local [Calibre](https://calibre-ebook.com/)
content server over WiFi. Implements issue #9.

---

## Architecture

The device acts as an HTTP client against a standard `calibre-server` instance on the
local network — the same "device pulls files over the LAN" pattern used by RomM for
game ROMs. No cloud relay, no companion app required (though the companion can configure
and trigger the sync remotely).

```
┌─────────────────────┐        WiFi / LAN        ┌──────────────────────┐
│   rsvpnano device   │ ─── HTTP GET ──────────▶ │  calibre-server      │
│   (ESP32-S3)        │ ◀── JSON / .rsvp bytes ─ │  port 8080 (default) │
│                     │                           │                      │
│  CalibreSyncManager │                           │  /ajax/library-info  │
│  CalibreClient      │                           │  /ajax/search        │
│  SD manifest        │                           │  /ajax/book/<id>     │
│  /books/.calibre-   │                           │  /get/rsvp/<id>/<lib>│
│    sync.json        │                           └──────────────────────┘
└─────────────────────┘
```

Key source files:

| File | Role |
|------|------|
| `src/calibre/CalibreClient.h/.cpp` | HTTP client + pure JSON parsers (`calibreparser` namespace) |
| `src/calibre/CalibreSettings.h/.cpp` | NVS-backed configuration (`CalibreSettings` struct) |
| `src/calibre/CalibreSettingsJson.h` | Pure serialize/parse helpers for the companion HTTP API (issue #12) |
| `src/sync/CalibreSyncPlan.h` | Pure, host-testable reconcile core (`calibresync::computeSyncPlan`) |
| `src/sync/CalibreSyncManager.h/.cpp` | On-device orchestrator: WiFi up → search → diff → download → delete → manifest rewrite → reindex |
| `src/settings/PreferenceKeys.h` | NVS key constants (`kPrefCal*`) |

The JSON parsers and reconcile core are deliberately free of WiFi/SD/clock dependencies so
they can be unit-tested on the host with the `test/support/Arduino.h` shim
(`test/calibre/`). The networking layer mirrors the `OtaUpdater` / `ReleaseParser` split.
The repo does **not** use ArduinoJson; all parsers hand-roll `String::indexOf` /
`String::substring` scanning, matching the existing `ReleaseParser` and `FeedParser`
style.

### Hardware requirement

All current targets use `board = esp32-s3-r8-opi` (ESP32-S3 with 8 MB octal-PSRAM).
The PSRAM is required to buffer HTTP response bodies for JSON parsing in heap without
exhausting IRAM. Do not attempt to port the Calibre sync to boards without PSRAM.
See `docs/esp32-s3-multi-target-layout.md` for the multi-board build layout.

---

## Server setup

### 1. Install the Calibre RSVP Output plugin

The device downloads `.rsvp` files. Calibre does not produce `.rsvp` natively; the
[calibre-rsvp-plugin](https://github.com/ionutdecebal/calibre-rsvp-plugin) adds RSVP
as an output format and registers the `application/x-rsvp` MIME type so the content
server serves it correctly.

```bash
calibre-customize -a RSVP_Output.zip
```

### 2. Convert books to RSVP

Select books in Calibre → **Convert books → Output format → RSVP**. This stores the
`.rsvp` alongside the other formats on each book.

### 3. Tag books for sync

The device only fetches books matching its `searchQuery` setting. The recommended
convention is the tag `rsvp`:

```
tag:rsvp
```

Add this tag to every book you want on the device. Any valid Calibre search expression
works — including saved searches (`search:<name>`). See the plugin README for details.

### 4. Start calibre-server

```bash
# Unauthenticated (trusted LAN):
calibre-server --port 8080 /path/to/Calibre\ Library

# With HTTP Basic auth:
calibre-server --port 8080 --enable-auth --manage-users /path/to/Calibre\ Library
```

The server listens on all interfaces and is reachable at `http://<host-lan-ip>:8080`.

---

## Device configuration

### On-device: Settings → Calibre

Navigate **Settings → Calibre** on the device to set:

| Field | Description |
|-------|-------------|
| **Server URL** | `http://<host-ip>:<port>` — no trailing slash |
| **Library ID** | Leave blank to use the server's `default_library` |
| **Search query** | e.g. `tag:rsvp` (default) |
| **Username / Password** | HTTP Basic credentials, if the server has `--enable-auth` |
| **Deletion policy** | `Mirror` (default) or `Keep` — see below |

Then tap **Sync from Calibre** to run an immediate sync.

### Via the companion app (issue #12)

The companion app configures Calibre sync remotely over the device's AP/STA HTTP server
using two routes:

| Method | Route | Purpose |
|--------|-------|---------|
| `GET` | `/api/calibre-settings` | Read current settings |
| `PUT` | `/api/calibre-settings` | Write settings |

JSON contract:

```json
{
  "enabled": true,
  "baseUrl": "http://192.168.0.120:8080",
  "searchQuery": "tag:rsvp",
  "username": "alice",
  "password": "",
  "libraryId": "",
  "deletionPolicy": "mirror"
}
```

`deletionPolicy` accepts `"mirror"` or `"keep"`. See the security note below regarding
the `password` field.

---

## Sync flow

`CalibreSyncManager::runSync()` executes the following steps:

1. **Connect WiFi** using the stored SSID/password (`kPrefWifiSsid` / `kPrefWifiPass`).
2. **Resolve library** — `GET /ajax/library-info` to confirm or discover `library_id`.
3. **Search** — `GET /ajax/search?query=<searchQuery>&library_id=<lib>` → `book_ids[]`.
4. **Resolve each book** — `GET /ajax/book/<id>?library_id=<lib>` → `RsvpRef` (url, size, mtime).
5. **Compute sync plan** — `calibresync::computeSyncPlan(remote, manifest, policy)` (pure, no I/O).
6. **Download** new and changed files to `/books/books/<sanitized-title>.rsvp` via streaming `net::get` (write to `.tmp`, then rename).
7. **Delete** removed books from SD (Mirror policy only).
8. **Rewrite manifest** `/books/.calibre-sync.json`.
9. **Reindex** — `StorageManager::refreshBooks()` so the library reflects the new/removed files.
10. **Tear down WiFi**.

Progress is reported via `ProgressCallback` with phases `"search"`, `"download"`,
`"delete"`, `"done"`, `"error"` and a `current/total` count for percentage display.

---

## Ajax endpoints

All requests are `GET`. Authentication uses HTTP Basic when credentials are configured.

### `GET /ajax/library-info`

```json
{
  "default_library": "Calibre_Library",
  "library_map": { "Calibre_Library": "Calibre_Library" }
}
```

Fields read by firmware: `.default_library`, `.library_map` (to validate a stored `library_id`).

### `GET /ajax/search?query=<q>&library_id=<lib>`

```json
{
  "book_ids": [1, 2, 3],
  "total_num": 3,
  "num": 3,
  "offset": 0
}
```

Fields read by firmware: `.book_ids[]`, `.total_num`, `.num`. When `total_num > num + offset`
there are more pages — append `&num=<n>&offset=<o>` to paginate.

### `GET /ajax/book/<id>?library_id=<lib>`

```json
{
  "title": "Example Book",
  "authors": ["Jane Doe"],
  "last_modified": "2026-06-17T15:37:34+00:00",
  "formats": ["epub", "rsvp"],
  "other_formats": {
    "rsvp": "/get/rsvp/1/rsvplib"
  },
  "format_metadata": {
    "rsvp": {
      "size": 475,
      "mtime": "2026-06-17T15:37:34.528479+00:00"
    }
  }
}
```

Fields read by firmware:

| Field | Type | Use |
|-------|------|-----|
| `other_formats.rsvp` | string | Relative download URL; absent when no `.rsvp` format exists — check key presence, not null |
| `format_metadata.rsvp.size` | integer (bytes) | Part of change-key |
| `format_metadata.rsvp.mtime` | ISO-8601 with sub-second precision | Part of change-key |
| `last_modified` | ISO-8601, second precision | Fallback timestamp when `mtime` absent |

Format names (in `.formats`, `.other_formats`, `.format_metadata`) are **lowercase**: `"rsvp"`, not `"RSVP"`.

### `GET /get/rsvp/<book_id>/<library_id>`

Downloads the `.rsvp` bytes. Note: `library_id` is a **path segment** here, not a query
parameter (unlike `/ajax/*` endpoints where it is `?library_id=<lib>`).

The firmware can use the path from `other_formats.rsvp` directly (prepend `baseUrl`) or
construct the URL from the template — both are equivalent.

---

## Incremental sync and the SD manifest

The manifest at `/books/.calibre-sync.json` tracks every file the device has downloaded
from Calibre. It is keyed by `book_id`:

```json
{
  "books": {
    "1": { "key": "475|2026-06-17T15:37:34.528479+00:00", "path": "/books/books/Example_Book.rsvp" },
    "2": { "key": "12048|2026-05-01T10:00:00.000000+00:00", "path": "/books/books/Another_Title.rsvp" }
  }
}
```

The **change-key** is the raw string concatenation `<size>|<mtime>`. The device has no
reliable RTC, so mtime is never parsed into an epoch — keys are compared byte-for-byte.
`mtime` is taken from `format_metadata.rsvp.mtime` (sub-second precision); if absent,
`last_modified` is used as fallback.

### Reconcile logic (`calibresync::computeSyncPlan`)

| Condition | Action |
|-----------|--------|
| Book in remote search, not in manifest | Download (new) |
| Book in both, keys differ | Download (changed) |
| Book in both, keys equal | Skip (unchanged) |
| Book in manifest, not in remote, policy = `Mirror` | Delete from SD |
| Book in manifest, not in remote, policy = `Keep` | Leave on SD |

### Deletion policy

- **Mirror** (default): the device mirrors the Calibre search scope exactly. Books removed
  from the search (untagged, deleted, or the query changed) are deleted from SD.
- **Keep**: downloaded files are never deleted. Use this if you want to read books offline
  after removing the `rsvp` tag from Calibre.

---

## NVS settings keys

All keys live in NVS namespace `"rsvp"` (15-char key limit). Defined in
`src/settings/PreferenceKeys.h`:

| Constant | Key | Type | Description |
|----------|-----|------|-------------|
| `kPrefCalEnabled` | `cal_en` | bool | Enable/disable sync |
| `kPrefCalUrl` | `cal_url` | String | Server base URL (trailing slash stripped on save) |
| `kPrefCalLibrary` | `cal_lib` | String | Library ID; empty = use server `default_library` |
| `kPrefCalQuery` | `cal_query` | String | Search query, e.g. `tag:rsvp` |
| `kPrefCalUser` | `cal_user` | String | HTTP Basic username (optional) |
| `kPrefCalPass` | `cal_pass` | String | HTTP Basic password (optional) |
| `kPrefCalDelPol` | `cal_delpol` | uint8_t | 0 = Mirror, 1 = Keep |

`loadCalibreSettings()` / `saveCalibreSettings()` open and close the NVS namespace
internally, following the same pattern as `CompanionSyncManager`.

---

## Security

- HTTP Basic credentials are transmitted in cleartext over plain HTTP. This is acceptable
  because `calibre-server` is a local, trusted-network service — the same model Calibre
  itself uses for its companion apps.
- Credentials are stored in NVS only. They are **never written to the SD card**.
- `GET /api/calibre-settings` always returns `"password": ""` — the stored password is
  never echoed back over the HTTP API. A `PUT` with an empty or absent `password` field
  preserves the stored credential (sentinel: empty string = "no change").
- If security on an untrusted LAN is required, run `calibre-server` behind a TLS
  terminator (e.g. nginx with a self-signed cert) and set `baseUrl` to `https://...`.
  The firmware's `net::get` selects `WiFiClientSecure` for `https://` URLs. **Note:** it
  currently calls `setInsecure()` (`HttpFetch.cpp`, `TODO(issue #4)`), so TLS *encrypts*
  the connection but does **not** verify the server certificate — i.e. no protection
  against an active MITM yet. Certificate pinning/verification is tracked as follow-up.

---

## Hardware feasibility

The sync engine requires the ESP32-S3 with PSRAM (`board = esp32-s3-r8-opi`, 8 MB octal
PSRAM). HTTP response bodies are buffered in heap for JSON parsing; without PSRAM the
heap available to Arduino tasks is insufficient for large search result payloads.

For the multi-board build layout that makes the ESP32-S3 target explicit at compile time,
see `docs/esp32-s3-multi-target-layout.md`. Board-specific pin assignments and display
geometry are in `src/platforms/<board>/BoardConfig.h`.
