import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
binary=root/"build/native-release-source/checks";binary.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(["g++","-std=c++23","-O1","-Wall","-Wextra",f"-I{root/'src'}",f"-I{root/'lib/HarfBuzz/upstream/src'}",f"-I{root/'lib/SheenBidi/upstream/Headers'}",
               str(Path(__file__).with_name("main.cpp")),"-o",str(binary)],check=True)
subprocess.run([str(binary)],check=True)
