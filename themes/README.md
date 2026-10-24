# RSVP Nano Themes

Copy this `themes` folder to the SD card root. The device loads `*.toml` files from `/themes`
at startup and appends valid themes after the built-in default theme.

The **Dyslexic background colour** theme (`dyslexic.toml`) uses a yellow background,
black text, and a blue anchor letter when focus highlighting is enabled.

Theme files are plain text:

```toml
name = "Catppuccin Mocha"

[colors]
background = "#1E1E2E"
foreground = "#CDD6F4"
muted = "#A6ADC8"
subtle = "#7F849C"
accent = "#F38BA8"
accentBar = "#F38BA8"
breakAccent = "#94E2D5"
onAccent = "#11111B"
surface = "#181825"
surfaceMuted = "#313244"
surfaceActive = "#45475A"
outline = "#585B70"
guide = "#6C7086"
guideFocus = "#F38BA8"
phantom = "#6C7086"
progressTrack = "#313244"
```

Colors are quoted `#RRGGBB` values; without quotes, `#` starts a TOML comment. Missing fields keep
the built-in defaults and unknown fields are ignored. Typography is configured independently in
device settings and per-book language font choices. Theme files do not use a schema version.
