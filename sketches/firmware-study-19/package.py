"""Package Study 19 as one offline HTML file; never builds or writes firmware."""
from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parent
html = (root / 'index.html').read_text()
for name in ('style.css', 'proposal.css', 'layout.css', 'books.css', 'storage.css'):
    html = html.replace(f'<link rel="stylesheet" href="{name}">', '<style>\n' + (root / name).read_text() + '\n</style>')
scripts = []
for name in ('book-text.js', 'chunks.js', 'app.js', 'translations.js', 'proposal.js', 'lessons.js', 'tuning.js', 'onboarding.js', 'companion-qr.js', 'books.js', 'storage.js'):
    script = (root / name).read_text()
    assert '</script' not in script.lower()
    scripts.append(script)
    html = html.replace(f'  <script src="{name}" defer></script>\n', '')
licenses = '\n\n'.join((root.parents[1] / 'third_party' / family / 'OFL.txt').read_text() for family in ('literata','atkinson-hyperlegible'))
html = html.replace('</body>', '<script>\n' + '\n'.join(scripts) + '\n</script>\n<script type="text/plain" id="font-licenses">' + licenses + '</script>\n</body>')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(html)
print(args.output)
