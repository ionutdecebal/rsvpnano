"""Decode production setup screenshots (requires Pillow and zxing-cpp)."""
import argparse
from pathlib import Path
from PIL import Image
import zxingcpp

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('screenshots', type=Path)
args = parser.parse_args()
expected = 'https://ionutdecebal.github.io/rsvpnano/'
paths = sorted(args.screenshots.glob('setup-*-books-*.png'))
assert len(paths) == 12, f'Expected two methods, three languages and two themes; got {len(paths)}'
paths += sorted(args.screenshots.glob('intro-*-10.png'))
assert len(paths) == 15, 'Missing tenth-lesson screenshots'
for path in paths:
    screen = Image.open(path).convert('RGB')
    assert screen.size == (640, 172)
    qr = screen.crop((16, 53, 127, 164))
    assert set(qr.getdata()) == {(0, 0, 0), (255, 255, 255)}, path
    for rectangle in [(0,0,111,12),(0,99,111,111),(0,0,12,111),(99,0,111,111)]:
        assert set(qr.crop(rectangle).getdata()) == {(255,255,255)}, (path, 'quiet zone')
    for y in range(12,99,3):
        for x in range(12,99,3):
            assert len(set(qr.crop((x,y,x+3,y+3)).getdata())) == 1, (path, 'module scale')
    codes = zxingcpp.read_barcodes(screen)
    assert len(codes) == 1 and codes[0].text == expected, (path, codes)
    print(f'PASS: {path.name}: native-size QR decodes; integer modules and quiet zone intact.')
print(f'{len(paths)} production native QR screenshots passed.')
