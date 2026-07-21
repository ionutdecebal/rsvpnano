#!/usr/bin/env python3
"""Generate embedded Cyrillic serif font headers for RSVP Nano book rendering."""

from __future__ import annotations

import argparse
import pathlib
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - build-time dependency
    raise SystemExit(
        "Pillow is required. Install with: pip install pillow"
    ) from exc


CYRILLIC_CODEPOINTS = [
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0401, 0x0416, 0x0417, 0x0418,
    0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422,
    0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C,
    0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0451,
    0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449,
    0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
]

DEFAULT_FONT_CANDIDATES = [
  pathlib.Path(r"C:\Windows\Fonts\times.ttf"),
  pathlib.Path(r"C:\Windows\Fonts\georgia.ttf"),
  pathlib.Path(r"C:\Windows\Fonts\arial.ttf"),
  pathlib.Path("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"),
  pathlib.Path("/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf"),
]

CANVAS_WIDTH = 112
CANVAS_HEIGHT = 128
ORIGIN_X = 10
BASELINE_Y = 76
ALPHA_THRESHOLD = 16
FONT_TOP_PADDING = 4
FONT_BOTTOM_PADDING = 2


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--point-size", type=int, default=52)
    parser.add_argument("--font-path", type=pathlib.Path, default=None)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--symbol-prefix", default="EmbeddedCyrillicSerif")
    return parser.parse_args()


def resolve_font(path: pathlib.Path | None, point_size: int) -> ImageFont.FreeTypeFont:
    candidates = [path] if path is not None else DEFAULT_FONT_CANDIDATES
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return ImageFont.truetype(str(candidate), point_size)
    raise FileNotFoundError("No suitable font found for Cyrillic glyph generation")


def render_glyph(font: ImageFont.FreeTypeFont, codepoint: int) -> Image.Image:
    image = Image.new("L", (CANVAS_WIDTH, CANVAS_HEIGHT), 255)
    draw = ImageDraw.Draw(image)
    draw.text((ORIGIN_X, BASELINE_Y - font.size), chr(codepoint), font=font, fill=0)
    return image


def glyph_bounds(image: Image.Image, crop_top: int, crop_bottom: int) -> tuple[int, int, int, int]:
    pixels = image.load()
    min_x = CANVAS_WIDTH
    max_x = -1
    for y in range(crop_top, crop_bottom + 1):
        for x in range(CANVAS_WIDTH):
            if 255 - pixels[x, y] > ALPHA_THRESHOLD:
                min_x = min(min_x, x)
                max_x = max(max_x, x)
    return min_x, max_x, crop_top, crop_bottom


def main() -> None:
    args = parse_args()
    font = resolve_font(args.font_path, args.point_size)

    images = [render_glyph(font, codepoint) for codepoint in CYRILLIC_CODEPOINTS]
    global_top = CANVAS_HEIGHT
    global_bottom = -1
    for image in images:
        pixels = image.load()
        for y in range(CANVAS_HEIGHT):
            for x in range(CANVAS_WIDTH):
                if 255 - pixels[x, y] > ALPHA_THRESHOLD:
                    global_top = min(global_top, y)
                    global_bottom = max(global_bottom, y)

    crop_top = max(0, global_top - FONT_TOP_PADDING)
    crop_bottom = min(CANVAS_HEIGHT - 1, global_bottom + FONT_BOTTOM_PADDING)
    font_height = crop_bottom - crop_top + 1

    bitmap_bytes: list[int] = []
    glyph_entries: list[str] = []

    for index, image in enumerate(images):
        min_x, max_x, _, _ = glyph_bounds(image, crop_top, crop_bottom)
        bitmap_offset = len(bitmap_bytes)
        pixels = image.load()

        if max_x >= min_x:
            glyph_width = max_x - min_x + 1
            for y in range(crop_top, crop_bottom + 1):
                for x in range(min_x, max_x + 1):
                    alpha = 255 - pixels[x, y]
                    bitmap_bytes.append(alpha if alpha > ALPHA_THRESHOLD else 0)
            x_offset = min_x - ORIGIN_X
            x_advance = max(8, max_x + 4)
        else:
            glyph_width = 0
            x_offset = 0
            x_advance = 8

        glyph_entries.append(
            "    "
            + "{"
            + f"{bitmap_offset}, {x_offset}, {glyph_width}, {x_advance}"
            + "}, "
            + f"// U+{CYRILLIC_CODEPOINTS[index]:04X}"
        )

    prefix = args.symbol_prefix
    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        f"// Generated Cyrillic serif glyphs for Russian book rendering.",
        f"// Source point size: {args.point_size} pt",
        "",
        f"struct {prefix}Glyph " + "{",
        "  uint32_t bitmapOffset;",
        "  int8_t xOffset;",
        "  uint8_t width;",
        "  uint8_t xAdvance;",
        "};",
        "",
        f"constexpr uint8_t k{prefix}Count = {len(CYRILLIC_CODEPOINTS)};",
        f"constexpr uint8_t k{prefix}Height = {font_height};",
        "",
        f"static const uint8_t k{prefix}Bitmaps[] PROGMEM = " + "{",
    ]

    for offset in range(0, len(bitmap_bytes), 16):
        chunk = bitmap_bytes[offset : offset + 16]
        lines.append("    " + ", ".join(f"{value:3d}" for value in chunk) + ",")

    lines += [
        "};",
        "",
        f"static const {prefix}Glyph k{prefix}Glyphs[] PROGMEM = " + "{",
        *glyph_entries,
        "};",
        "",
    ]

    args.output.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"Wrote {args.output} ({len(CYRILLIC_CODEPOINTS)} glyphs, height={font_height})")


if __name__ == "__main__":
    main()
