"""Capture watch screens using firmware layouts, UI font, and the real Arduino_GFX rasterizer.

Host-only canvases do not change device rendering or allocate device memory.
Requires the installed PlatformIO native and AMOLED 1.8 V2 dependencies.
Run `pio test -e native_watch_test` first; use --native-build if its build directory was overridden.
"""

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--compiler", default=shutil.which("g++"))
    parser.add_argument("--native-build", type=Path, default=PROJECT / ".pio/build/native_watch_test")
    args = parser.parse_args()
    if not args.compiler:
        parser.error("Use --compiler to select a native g++ compiler")
    support = Path(__file__).resolve().parent
    library = PROJECT / ".pio/libdeps/waveshare_esp32s3_touch_amoled_18_v2/GFX Library for Arduino/src"
    sources = [
        "ui/Ui.cpp", "ui/Controls.cpp", "ui/Icons.cpp", "ui/Theme.cpp", "ui/Localization.generated.cpp",
        "localization/LocalePack.cpp", "text/LocaleTag.cpp", "text/BidiText.cpp", "text/TextShaping.cpp",
        "reader/ReadingLoop.cpp", "ui/screens/watch/ScreenCommon.cpp", "ui/screens/watch/ReadScreen.cpp",
        "ui/screens/watch/SettingsScreen.cpp", "ui/screens/watch/DeviceScreen.cpp",
        "ui/screens/watch/ReadingSettingsScreen.cpp", "ui/screens/watch/PacingSettingsScreen.cpp",
        "ui/screens/watch/ChaptersScreen.cpp", "ui/screens/watch/OtaScreen.cpp",
        "ui/screens/ReaderLayout.cpp", "ui/screens/watch/ReaderLayout.cpp",
    ]
    includes = [support, PROJECT / "test/native_canvas", PROJECT / "test/support", PROJECT / "src", library,
                PROJECT / "lib/HarfBuzz/upstream/src", PROJECT / "lib/SheenBidi/upstream/Headers",
                PROJECT / ".pio/libdeps/native_watch_test/glaze/include"]
    libraries = []
    for name in ("HarfBuzz", "SheenBidi"):
        archive = next(args.native_build.glob(f"lib*/lib{name}.a"), None)
        if archive is None:
            parser.error(f"Missing {name} archive in {args.native_build}; build native_watch_test first")
        libraries.append(archive)
    with tempfile.TemporaryDirectory(prefix="rsvpnano-watch-preview-") as temp:
        executable = Path(temp) / "watch-preview.exe"
        subprocess.run([
            args.compiler, "-std=c++23", "-O1", "-funsigned-char", "-ffunction-sections", "-fdata-sections",
            "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
            *[f"-I{path}" for path in includes], str(support / "main.cpp"),
            *[str(PROJECT / "src" / file) for file in sources],
            str(library / "Arduino_G.cpp"), str(library / "Arduino_GFX.cpp"),
            str(library / "canvas/Arduino_Canvas.cpp"), *map(str, libraries), "-Wl,--gc-sections", "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable), str(args.output.resolve())], check=True)


if __name__ == "__main__":
    main()
