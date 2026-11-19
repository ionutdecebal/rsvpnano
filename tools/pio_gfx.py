"""Apply board-specific fixes to generated copies of the pinned graphics library."""

from pathlib import Path


def patch_u8g2_coordinates(source: str) -> str:
    signature = "void Arduino_GFX::u8g2_font_decode_len("
    if source.count(signature) != 1:
        raise RuntimeError("Arduino_GFX u8g2 decoder changed; review the pinned dependency patch")
    start = source.index(signature)
    end = source.index("\n}", start)
    function = source[start:end]
    old = "  /* target position on the screen */\n  uint16_t x, y;"
    new = "  /* target position on the screen */\n  int16_t x, y;"
    if function.count(new) == 1 and old not in function:
        return source
    if function.count(old) != 1 or new in function:
        raise RuntimeError("Arduino_GFX u8g2 coordinates changed; review the pinned dependency patch")
    return source[:start] + function.replace(old, new, 1) + source[end:]


def signed_u8g2_source(env, node):
    source_path = Path(node.srcnode().get_abspath())
    source = source_path.read_text(encoding="utf-8")
    corrected = patch_u8g2_coordinates(source)
    if corrected == source:
        return node
    return generated_source(env, source_path, corrected)


def generated_source(env, source_path, corrected):
    generated = Path(env.subst("$BUILD_DIR")) / "generated" / source_path.name
    generated.parent.mkdir(parents=True, exist_ok=True)
    if not generated.exists() or generated.read_text(encoding="utf-8") != corrected:
        generated.write_text(corrected, encoding="utf-8")
    return env.File(str(generated))


def lcd349_qspi_source(env, node):
    path = Path(node.srcnode().get_abspath())
    source = path.read_text(encoding="utf-8")
    signature = "void Arduino_ESP32QSPI::writePixels(uint16_t *data, uint32_t len)"
    if source.count(signature) != 1:
        raise RuntimeError("Review the LCD pixel transfer against the pinned Arduino_GFX driver")
    start = source.index(signature)
    end = source.index("\n}", start) + 2
    replacement = Path(env.subst("$PROJECT_DIR")) / "tools/gfx/lcd349_write_pixels.inc"
    source = source[:start] + f'#include "{replacement.as_posix()}"' + source[end:]
    # The generated unit lives outside the original databus directory.
    source = source.replace('#include "Arduino_ESP32QSPI.h"', '#include "databus/Arduino_ESP32QSPI.h"')
    return generated_source(env, path, source)


if "Import" in globals():
    Import("env")
    env.AddBuildMiddleware(signed_u8g2_source, "*/Arduino_GFX.cpp")
    if any("RSVP_LCD_349_REVISION_HEADER" in flag for flag in env.GetProjectOption("build_flags")):
        env.AddBuildMiddleware(lcd349_qspi_source, "*/databus/Arduino_ESP32QSPI.cpp")
