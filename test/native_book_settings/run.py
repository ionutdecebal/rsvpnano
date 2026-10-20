"""Run real per-book persistence/editor code against an in-memory SD fixture."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root / "build/native-book-settings"
out.mkdir(parents=True, exist_ok=True)
subprocess.run([
    "g++", "-std=c++23", "-O1", "-ffunction-sections", "-fdata-sections",
    "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
    f"-I{root / 'test/native_storage_checks'}", f"-I{root / 'src'}",
    f"-I{root / 'test/support'}", f"-I{root / '.pio/libdeps/native_test/glaze/include'}",
    f"-I{root / 'lib/HarfBuzz/upstream/src'}", f"-I{root / 'lib/SheenBidi/upstream/Headers'}",
    str(Path(__file__).with_name("main.cpp")),
    *[str(root / "src" / p) for p in (
        "library/BookSettingsEditor.cpp", "library/ReadingProgress.cpp",
        "library/StorageManager.cpp", "storage/fs/StorageFiles.cpp",
    )],
    "-Wl,--gc-sections", "-o", str(out / "checks"),
], check=True)
subprocess.run([str(out / "checks")], check=True)
