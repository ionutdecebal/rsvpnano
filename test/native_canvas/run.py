"""Compare the installed native Arduino_GFX u8g2 decoder on full and two-row canvases.

Run from any directory after installing firmware dependencies:
    python test/native_canvas/run.py
Use --library /path/to/Arduino_GFX/src for an independent checkout.
Use --fetch-library to fetch the exact dependency revision in platformio.ini.
"""

import argparse
import configparser
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "tools"))
from pio_gfx import patch_u8g2_coordinates


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    library_option = parser.add_mutually_exclusive_group()
    library_option.add_argument("--library", type=Path)
    library_option.add_argument("--fetch-library", action="store_true")
    parser.add_argument("--compiler", default=shutil.which("g++") or shutil.which("c++"))
    parser.add_argument("--unpatched", action="store_true", help="Reproduce the original decoder failure")
    args = parser.parse_args()
    if not args.compiler:
        parser.error("A native C++ compiler is required; use --compiler")
    support = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="rsvpnano-native-canvas-") as temporary:
        folder = Path(temporary)
        if args.fetch_library:
            config = configparser.ConfigParser(interpolation=None)
            config.read(PROJECT / "platformio.ini", encoding="utf-8")
            dependency = next(
                line.strip() for line in config["common"]["base_lib_deps"].splitlines()
                if "Arduino_GFX.git#" in line
            )
            repository, revision = dependency.rsplit("#", 1)
            checkout = folder / "Arduino_GFX"
            subprocess.run(["git", "init", "--quiet", str(checkout)], check=True)
            subprocess.run(
                ["git", "-C", str(checkout), "fetch", "--quiet", "--depth=1", repository, revision], check=True
            )
            subprocess.run(
                ["git", "-C", str(checkout), "checkout", "--quiet", "--detach", "FETCH_HEAD"], check=True
            )
            library = checkout / "src"
        else:
            library = (
                args.library
                or PROJECT / ".pio/libdeps/waveshare_esp32s3_touch_amoled_18_v2/GFX Library for Arduino/src"
            ).resolve()
        source = (library / "Arduino_GFX.cpp").read_text(encoding="utf-8")
        corrected = patch_u8g2_coordinates(source)
        assert patch_u8g2_coordinates(corrected) == corrected, "Patch must be idempotent"
        for unexpected in (
            source.replace("u8g2_font_decode_len(", "different_decoder("),
            corrected.replace("  int16_t x, y;", "  int32_t x, y;"),
        ):
            try:
                patch_u8g2_coordinates(unexpected)
            except RuntimeError:
                pass
            else:
                raise AssertionError("Unexpected upstream source must fail closed")
        generated = folder / "Arduino_GFX.cpp"
        generated.write_text(source if args.unpatched else corrected, encoding="utf-8")
        executable = folder / "native_canvas_text.exe"
        command = [
            args.compiler, "-std=c++17", "-O2", f"-I{support}", f"-I{PROJECT / 'src'}", f"-I{library}",
            str(support / "main.cpp"), str(library / "Arduino_G.cpp"), str(generated),
            str(library / "canvas/Arduino_Canvas.cpp"), "-o", str(executable),
        ]
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
