"""Render the LCD UI with the real Arduino_GFX rasterizer, not an HTML imitation.

Run native_test first to build HarfBuzz and SheenBidi, and install the LCD rev1
PlatformIO dependencies. Only book metadata and hardware are fixtures.
"""
import argparse
import shutil
import subprocess
import hashlib
import shlex
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    library = PROJECT / ".pio/libdeps/waveshare_esp32s3_touch_lcd_349_rev1/GFX Library for Arduino/src"
    sources = [
        "ui/screens/NetworkSettingsScreen.cpp", "ui/screens/regular/NetworkSettingsScreen.cpp",
        "ui/screens/StandbyScreen.cpp",
        "dictionary/DictionaryScreen.cpp", "ui/screens/ReaderChunks.cpp", "ui/screens/ReaderTypeface.cpp", "ui/screens/ReaderPlayback.cpp", "ui/screens/ReaderText.cpp", "reader/ContextChunks.cpp", "ui/Logo.cpp", "ui/screens/StatusAnimation.cpp", "ui/Keyboard.cpp", "ui/screens/PageReaderScreen.cpp", "ui/screens/ReaderNavigation.cpp", "ui/screens/ModernFocusScreen.cpp", "ui/Ui.cpp", "ui/SansFont.cpp", "ui/Modern.cpp", "ui/Controls.cpp", "ui/Icons.cpp", "ui/Theme.cpp",
        "ui/Localization.generated.cpp", "localization/LocalePack.cpp", "text/LocaleTag.cpp", "text/BidiText.cpp",
        "text/TextShaping.cpp", "reader/ReadingLoop.cpp", "ui/screens/ModernScreens.cpp",
        "ui/screens/regular/ScreenCommon.cpp", "ui/screens/regular/SettingsScreen.cpp",
        "ui/screens/regular/LibraryScreen.cpp", "ui/screens/regular/ChaptersScreen.cpp",
        "ui/screens/regular/DeviceScreen.cpp", "ui/screens/regular/ReadingSettingsScreen.cpp",
        "ui/screens/regular/PacingSettingsScreen.cpp", "ui/screens/regular/OtaScreen.cpp",
        "ui/screens/regular/StatusScreen.cpp", "ui/screens/regular/BookFontsScreen.cpp",
        "ui/screens/ModernReaderChrome.cpp", "ui/screens/ReaderLayout.cpp", "ui/screens/regular/ReaderLayout.cpp",
        "ui/screens/regular/InterfaceSettingsScreen.cpp", "ui/screens/InterfaceSettingsScreen.cpp",
        "ui/screens/ModernAppearanceScreen.cpp", "ui/screens/regular/FocusScreen.cpp",
        "focus/FocusTimers.cpp", "focus/FocusSession.cpp",
        "ui/screens/ModernReaderControls.cpp", "ui/screens/OnboardingCopy.cpp", "ui/screens/OnboardingScreen.cpp", "ui/screens/OnboardingStorageScreen.cpp", "ui/screens/BookUploadScreen.cpp", "ui/screens/TouchCheckScreen.cpp", "drivers/touch/axs15231b_touch/axs15231b_touch.cpp",
    ]
    sources += [str(p.relative_to(PROJECT / "src")) for p in (PROJECT / "src/screensavers").glob("*.cpp")]
    includes = [PROJECT / "test/native_modern_preview", PROJECT / "test/native_watch_preview", PROJECT / "test/native_canvas", PROJECT / "test/support",
                PROJECT / "src", library, PROJECT / "lib/HarfBuzz/upstream/src",
                PROJECT / "lib/SheenBidi/upstream/Headers", PROJECT / ".pio/libdeps/native_test/glaze/include"]
    archives = [next((PROJECT / ".pio/build/native_test").glob(f"lib*/lib{name}.a"))
                for name in ("HarfBuzz", "SheenBidi")]
    flags = [shutil.which("g++"), "-std=c++23", "-O1", "-funsigned-char", "-ffunction-sections",
             "-fdata-sections", "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
             *[f"-I{p}" for p in includes]]
    # Compiler dependency files keep iterative device-layout review incremental.
    # The cache is disposable build output, never a firmware/source dependency.
    cache = PROJECT / "build/native-modern-preview" / hashlib.sha256(repr(flags).encode()).hexdigest()[:12]
    cache.mkdir(parents=True, exist_ok=True)
    units = [Path(__file__).with_name("main.cpp"), Path(__file__).with_name("Fixtures.cpp"), Path(__file__).with_name("DictionaryFixture.cpp"), *[PROJECT / "src" / p for p in sources],
             library / "Arduino_G.cpp", library / "Arduino_GFX.cpp", library / "canvas/Arduino_Canvas.cpp"]

    def compile_unit(source):
        obj = cache / (source.stem + "-" + hashlib.sha256(str(source).encode()).hexdigest()[:10] + ".o")
        dep = obj.with_suffix(".d")
        rebuild = not obj.exists() or not dep.exists()
        if not rebuild:
            dependencies = shlex.split(dep.read_text().replace("\\\n", "").split(": ", 1)[1])
            rebuild = any(not Path(p).exists() or Path(p).stat().st_mtime_ns > obj.stat().st_mtime_ns for p in dependencies)
        if rebuild:
            print(f"Compiling preview: {source.name}", flush=True)
            subprocess.run([*flags, "-MMD", "-MF", str(dep), "-c", str(source), "-o", str(obj)], check=True)
        return obj

    with ThreadPoolExecutor(max_workers=2) as pool:
        objects = list(pool.map(compile_unit, units))
    binary = cache / "preview"
    if not binary.exists() or any(p.stat().st_mtime_ns > binary.stat().st_mtime_ns for p in [*objects, *archives]):
        subprocess.run([flags[0], *map(str, objects), *map(str, archives), "-Wl,--gc-sections", "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(args.output.resolve())], check=True)
    # Lossless screenshots for review; no resampling or synthetic browser text.
    from PIL import Image
    for ppm in args.output.glob("*.ppm"):
        Image.open(ppm).save(ppm.with_suffix(".png"))


if __name__ == "__main__":
    main()
