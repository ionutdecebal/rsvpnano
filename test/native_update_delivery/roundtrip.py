"""Compile each checkout's real settings codec and verify New UI -> Classic compatibility."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2]
classic=Path(sys.argv[1])
out=root/"build/native-update-delivery";out.mkdir(parents=True,exist_ok=True)
for tree, name, flags in ((root,"new",["-DGENERATE_NEW_UI"]), (classic,"classic",[])):
    subprocess.run(["g++","-std=c++23","-O1","-DGLZ_DEFAULT_OPTIMIZATION_SIZE","-DGLZ_DISABLE_ALWAYS_INLINE",
        f"-I{tree/'src'}",f"-I{root/'test/support'}",f"-I{root/'.pio/libdeps/native_test/glaze/include'}",
        f"-I{root/'lib/HarfBuzz/upstream/src'}",f"-I{root/'lib/SheenBidi/upstream/Headers'}",
        *flags,str(Path(__file__).with_name("settings_roundtrip.cpp")),
        str(tree/"src/settings/SettingsCodec.cpp"),"-o",str(out/name)],check=True)
with (out/"new-ui.toml").open("w") as file:
    subprocess.run([str(out/"new")],stdout=file,check=True)
with (out/"classic.toml").open("w") as file:
    subprocess.run([str(out/"classic"),str(out/"new-ui.toml")],stdout=file,check=True)
print("Cross-checkout settings: New UI chunk settings load in Classic as RSVP with speed and Wi-Fi retained.")
