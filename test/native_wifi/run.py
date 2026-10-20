from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temp:
    binary=str(Path(temp)/'wifi-test')
    subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-I'+str(root/'test/native_wifi'),'-I'+str(root/'src'),'-I'+str(root/'lib/HarfBuzz/upstream/src'),'-I'+str(root/'lib/SheenBidi/upstream/Headers'),
        str(root/'test/native_wifi/main.cpp'),str(root/'src/network/WifiConnection.cpp'),'-o',binary],check=True)
    subprocess.run([binary],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
