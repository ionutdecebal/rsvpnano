"""Exercise production settings persistence and library caching with injected I/O.

Hardware NVS/FS, book indexing and SD mounting are fixtures. No device is touched.
"""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "build/native-storage-checks"
OUT.mkdir(parents=True, exist_ok=True)
subprocess.run([
    "g++", "-std=c++23", "-O1", "-ffunction-sections", "-fdata-sections",
    "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
    f"-I{HERE}", f"-I{ROOT / 'src'}", f"-I{ROOT / 'test/support'}",
    f"-I{ROOT / '.pio/libdeps/native_test/glaze/include'}",
    f"-I{ROOT / 'lib/HarfBuzz/upstream/src'}",
    f"-I{ROOT / 'lib/SheenBidi/upstream/Headers'}",
    str(HERE / "main.cpp"),
    *[str(ROOT / "src" / source) for source in (
        "settings/SettingsStore.cpp", "settings/SettingsCodec.cpp", "text/LocaleTag.cpp",
        "library/StorageManager.cpp", "ui/screens/LibraryScreen.cpp",
    )],
    "-Wl,--gc-sections", "-o", str(OUT / "checks"),
], check=True)
subprocess.run([str(OUT / "checks")], check=True)
