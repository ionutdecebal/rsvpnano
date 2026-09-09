# Configuration

RSVP Nano stores human-editable configuration as versionless TOML. Glaze reads and writes the
same plain aggregates used by the firmware and exposes device settings as JSON to companion
clients.

## Compatibility

- Missing fields keep their firmware defaults.
- Unknown fields are ignored. Canonical settings writes omit them.
- Settings loaded from SD or NVS are rewritten canonically, so newly added fields appear in
  `/config/settings.toml` and obsolete fields disappear.
- There is no `schemaVersion` field. An old one is treated like any other unknown field.
- Malformed TOML, wrong value types, and invalid enum names still fail instead of partially
  changing live settings.
- Bounded numeric settings are clamped to their supported range.

The SD settings file is limited to 8 KiB. If it is malformed, the firmware preserves it for the
user to repair and continues with valid NVS settings or defaults.

## Device Settings

`/config/settings.toml` mirrors the public device settings. A partial file is valid; the next
successful load expands it to the full canonical document. For example:

```toml
[reading]
wpm = 350
batteryLabel = "percentage"
batteryIconVisible = true

[reading.typography]
fontId = "literata"
focusHighlight = true

[interface]
brightnessPercent = 70
selectedThemeId = "default"

[network]
wifiSsid = "Home"
```

Runtime code reads settings from `SettingsStore` in RAM. Accepted changes are saved after a short
debounce rather than writing flash for every UI step. NVS stores the same canonical TOML as one
durable blob and is used when the SD mirror is unavailable.

The Wi-Fi password is not part of `DeviceSettings`, the SD file, or companion settings JSON. It is
stored separately in the NVS-only secrets document.

## RSS Feeds

`/config/rss.toml` contains up to 24 unique HTTP or HTTPS feed URLs:

```toml
feeds = ["https://example.com/feed.xml", "https://example.org/rss"]
```

Whitespace and duplicate URLs are removed when the configuration is saved.

Each check visits up to the first eight configured feeds and saves at most 12 new articles
in total. Already-synced articles and empty entries do not consume that allowance; run
another check to continue through the articles still present in the feed. Downloads are
limited to 4 MiB per feed and article text to 512 KiB. Complete entries from a partial
download remain usable if the feed reaches the size limit or times out.

## Focus Timers

`/config/focus.toml` contains up to six timers:

```toml
[[timers]]
name = "Pomodoro"
focusMinutes = 25
breakMinutes = 5
rounds = 4
```

Timer names are limited to 14 UTF-8 bytes. Durations and round counts are clamped to their
supported ranges.

## Themes And Book State

Theme files are versionless TOML under `/themes`; see [`../themes/README.md`](../themes/README.md).
Each book can also have a hidden `.rstate.toml` sidecar containing durable progress and an optional
typography override. Book state is separate from global device settings.
