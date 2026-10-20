"""Exercise release validation and real dictionary transaction code with SD fault injection."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / "build/native-update-delivery"
out.mkdir(parents=True, exist_ok=True)
subprocess.run(["g++", "-std=c++23", "-O1", "-Wall", "-Wextra",
    "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
    f"-I{root/'src'}", f"-I{root/'test/native_storage_checks'}",
    f"-I{root/'.pio/libdeps/native_test/glaze/include'}",
    str(Path(__file__).with_name("main.cpp")), str(root/"src/update/ChannelCatalog.cpp"),
    "-o", str(out/"checks")], check=True)
subprocess.run([str(out/"checks")], check=True)
subprocess.run(["g++", "-std=c++23", "-O1", "-Wall", "-Wextra",
    f"-I{root/'test/native_update_delivery/support'}", f"-I{root/'src'}",
    str(Path(__file__).with_name("transport.cpp")), str(root/"src/network/VerifiedDownload.cpp"),
    "-o", str(out/"transport")], check=True)
subprocess.run([str(out/"transport")], check=True)
subprocess.run(["g++", "-std=c++23", "-O1", "-Wall", "-Wextra",
    "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
    f"-I{root/'test/native_update_delivery/finalize_support'}", f"-I{root/'src'}",
    f"-I{root/'.pio/libdeps/native_test/glaze/include'}",
    str(Path(__file__).with_name("finalize.cpp")), str(root/"src/update/SwitchSettings.cpp"),
    "-o", str(out/"finalize")], check=True)
subprocess.run([str(out/"finalize")], check=True)
