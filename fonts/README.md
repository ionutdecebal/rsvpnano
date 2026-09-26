# RSVP Nano fonts

This directory holds the offline font pipeline and the pre-converted `.rfont4` catalog used by the web flasher/companion.
The compiled Literata fallback is intentionally absent from this installable catalog.

`RFont4` is named for its primary packed 4-bit alpha coverage. Format version 8 stores four Alpha4 RSVP strikes, an Alpha4 preview strike, and
one compact 1-bit strike in the same family file. Codepoints, Unicode page tables, a direct OpenType glyph-ID map,
and the generated script-capability mask are stored once per family; each strike contains only its metrics, kerning,
and packed pixels. Alpha4 glyphs are independently LZ4-compressed when that saves space, preserving random access;
the compact strike remains raw. The renderer finds visible spans after each row is read instead of storing duplicate indexes.
Files from versions before version 8 must be regenerated.

Folder layout:

```text
third_party/
  atkinson-hyperlegible/
    AtkinsonHyperlegible-Regular.ttf
    OFL.txt
fonts/
  convert_alpha4_font.py
  generate_fonts.py
  index.json
  Atkinson Hyperlegible/
    font.rfont4
    OFL.txt
```

The generated fallback header is the one font artifact that belongs under `src` because it is compiled into firmware:

```text
src/fonts/
  LiterataFallbackAlpha4.h
```

`convert_alpha4_font.py` is the single-family compiler. `large`, `medium`, `small`, `compact`, `extra-large`, and `preview` default to
`52`, `43`, `33`, `14`, `80`, and `26` px. Existing size indices remain unchanged.
The reader accepts older four-strike version 8 files, using their Large/Compact strikes for the new sizes until updated.
Extra Large fits within the reader height; Large and Extra Large use three rows in the swipe page preview.
The preview selects the largest fitting strike for each font family. The compact strike remains 1-bit, but 14 px gives outline reader faces a readable
minimum comparable to the hand-drawn 8–10 px UI fonts. Override the sizes with:

```bash
uv run --with freetype-py --with fonttools --with lz4 python fonts/convert_alpha4_font.py \
  --font path/to/MyFont.ttf \
  --name "My Font" \
  --sizes large=56,medium=44,small=34,compact=14,extra-large=80,preview=26
```

`generate_fonts.py` owns the current source-to-preset mapping. With no arguments it regenerates every installable
font and the compiled Literata fallback, publishing only after every requested conversion succeeds:

```bash
uv run --with freetype-py --with fonttools --with lz4 python fonts/generate_fonts.py
```

Pass preset ids to regenerate only selected families, or use `--list` to show them:

```bash
uv run --with freetype-py --with fonttools --with lz4 python fonts/generate_fonts.py amiri noto-serif-japanese
```

Reader source binaries and their licenses live under `third_party/<family>/`. Generated files and the license files
distributed with them live under `fonts/<display name>/`.

Add `--shaping` only for fonts whose scripts require contextual OpenType shaping. It embeds the subsetted
GDEF/GSUB/GPOS tables and glyph-ID closure once in the family file; ordinary Latin/Cyrillic fonts omit both.

Use `--locales` only as font-selection affinity; it does not install UI strings or make the font depend on a locale
pack. `--scripts` explicitly declares complete ISO 15924 capabilities and overrides cmap inference, preventing
incidental glyphs from advertising a whole script. Add `Zmth` only for a font intended to provide complete Unicode
math coverage rather than a font that happens to contain a few operators:

The named reader maps `Hebr`, `Arab`, `Hani`, `Jpan`, and `Zmth` select only their required script codepoints plus
reader punctuation, digits, currency symbols, and bidi controls. They also declare their script capabilities, so a
duplicate `--scripts` argument is unnecessary. Unrelated letters from multilingual source fonts are deliberately
omitted.

```bash
uv run --with freetype-py --with fonttools --with lz4 python fonts/convert_alpha4_font.py \
  --font path/to/NotoSerifHebrew-Regular.ttf \
  --name "Noto Serif Hebrew" \
  --map Hebr \
  --locales he \
  --shaping

uv run --with freetype-py --with fonttools --with lz4 python fonts/convert_alpha4_font.py \
  --font path/to/STIXTwoMath-Regular.otf \
  --name "STIX Two Math" \
  --map Zmth

uv run --with freetype-py --with fonttools --with lz4 python fonts/convert_alpha4_font.py \
  --font path/to/NotoSerifJP-Regular.otf \
  --name "Noto Serif Japanese" \
  --map Jpan \
  --locales ja
```

Horizontal CJK fonts intentionally omit `--shaping`; direct cmap glyphs cover this renderer, while vertical layout
is outside the current engine. Arabic and Hebrew retain their compact GDEF/GSUB/GPOS sections.

Default output is `fonts/<name>/font.rfont4` plus an updated `fonts/index.json`.

Generate the built-in fallback header only when firmware fallback data must change. By default this writes to `src/fonts/LiterataFallbackAlpha4.h`:

```bash
uv run --with freetype-py --with fonttools --with lz4 python fonts/convert_alpha4_font.py \
  --font third_party/literata/Literata-Regular.ttf \
  --name LiterataFallbackAlpha4 \
  --header
```

Runtime behavior:

- The SD card catalog is built from `/fonts/<folder name>/` directories.
- Each folder contains one `font.rfont4` family file with Extra Large, Large, Medium, Small, and Preview Alpha4
  strikes plus one Compact 1-bit strike. Compact is selectable in RSVP mode and is used for page reading.
  Large and Extra Large scrub previews use three rows and select the largest strike that fits each row.
- Optional GDEF/GSUB/GPOS shaping tables are stored once in that same file, never in a locale pack.
- BMP page tables map codepoints directly to shared render glyphs; only supplementary codepoints have separate lookup
  records. Identical bitmap payloads and kerning slices are interned within each strike.
- Japanese and Chinese families store one optional sparse vertical-rule section shared by all strikes. Upright glyphs
  have no rule, rotated glyphs reuse their canonical bitmap, and `vert`/`vrt2` alternates add pixels only when the source
  font actually supplies a distinct glyph. The section's presence is the capability signal.
- The selected family file stays open once. On PSRAM boards, shared lookup metadata and the active strike are loaded
  once when space permits. Otherwise, the 256-byte page map remains resident and the bounded block/bitmap caches
  service lookup data and packed rows from the file.
- Each bundled font keeps its license and any upstream font log beside its converted assets.
- Generated RFont4 files include the rasterized glyph closure referenced by their shared layout tables, preserving
  the source font's OpenType glyph IDs.
- Locale-pack U8g2 fonts live under `/locales/<id>/ui`, contain only fixed translated UI strings, and are never
  loaded by the reader. Arbitrary book text uses the selected RFont4 family's compact strike.
- If the selected RFont4 lacks a glyph, the renderer falls through to the compiled U8g2 font without loading another asset.
- Installing an RFont4 is sufficient to enable its reader scripts. Installing the matching UI locale is optional.

Validation is split deliberately: the converter and native tests verify complete generated assets, while firmware
checks the installed-file header and section layout before activation and bounds-checks each record it reads.
