"""Keep native u8g2 decoding signed when glyphs cross a canvas edge."""

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
    generated = Path(env.subst("$BUILD_DIR")) / "generated" / "Arduino_GFX.cpp"
    generated.parent.mkdir(parents=True, exist_ok=True)
    if not generated.exists() or generated.read_text(encoding="utf-8") != corrected:
        generated.write_text(corrected, encoding="utf-8")
    # PlatformIO compiles the replacement with the original library environment.
    return env.File(str(generated))


if "Import" in globals():
    Import("env")
    env.AddBuildMiddleware(signed_u8g2_source, "*/Arduino_GFX.cpp")
