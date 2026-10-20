"""Check bounded SD lookups against the exact redistributable dictionary pack."""
import argparse, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument("pack",type=Path);a=p.parse_args()
binary=root/"build/native-dictionary/checks";binary.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(["g++","-std=c++23","-O1","-Wall","-Wextra",f"-I{root/'src'}",
               str(Path(__file__).with_name("main.cpp")),"-o",str(binary)],check=True)
subprocess.run([str(binary),str(a.pack)],check=True)
