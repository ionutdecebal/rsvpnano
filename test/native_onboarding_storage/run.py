"""Run the real onboarding storage checker with an in-memory filesystem and I/O faults."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
here=Path(__file__).resolve().parent
out=root/'build/native-onboarding-storage'
out.mkdir(parents=True,exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-Wall','-Wextra',f'-I{here}',f'-I{root/"src"}',
    str(here/'main.cpp'),str(root/'src/storage/fs/OnboardingStorage.cpp'),'-o',str(out/'checks')],check=True)
subprocess.run([str(out/'checks')],check=True)
