from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/native-update-catalog'
out.mkdir(parents=True,exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-Wall','-Wextra','-DRSVP_CLASSIC_BUILD',
    '-DGLZ_DEFAULT_OPTIMIZATION_SIZE','-DGLZ_DISABLE_ALWAYS_INLINE',
    f'-I{root/"src"}',f'-I{root/".pio/libdeps/native_test/glaze/include"}',
    str(Path(__file__).with_name('main.cpp')),str(root/'src/update/ChannelCatalog.cpp'),
    '-o',str(out/'check')],check=True)
subprocess.run([str(out/'check')],check=True)
