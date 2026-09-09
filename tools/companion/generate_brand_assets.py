# /// script
# dependencies = ["resvg-py==0.3.3", "pillow>=11"]
# ///
"""Regenerate web and native artwork: uv run tools/companion/generate_brand_assets.py."""
from copy import deepcopy
from io import BytesIO
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw
import resvg_py

ROOT = Path(__file__).resolve().parents[2]
BRAND = ROOT / "companion/assets/brand"
STORE = ROOT / "companion/assets/store"
WEB = ROOT / "companion/apps/web/src/wasmJsMain"
NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", NS)


def read(name):
    return ET.parse(BRAND / name).getroot()


def serialize(svg):
    return ET.tostring(svg, encoding="unicode")


def write(path, text):
    path.write_text(text + "\n", encoding="utf-8")


def raster(svg, size):
    return Image.open(BytesIO(resvg_py.svg_to_bytes(svg_string=serialize(svg), width=size, height=size))).convert("RGBA")


icon = read("RSVP-Companion-App-Icon.svg")
icon.set("width", "1080")
icon.set("height", "1080")
foreground = deepcopy(icon)
for group in foreground.iter(f"{{{NS}}}g"):
    for child in list(group):
        if "url(" in child.get("style", ""):
            group.remove(child)
background = deepcopy(icon)
for group in background.iter(f"{{{NS}}}g"):
    for child in list(group):
        if child.tag == f"{{{NS}}}path" or (child.tag == f"{{{NS}}}rect" and child.get("style", "").startswith("fill:") and "url(" not in child.get("style", "")):
            group.remove(child)

# Keep source drawables at their supplied size. Android's adaptive-icon XML
# owns the 18% inset; mirror that composition only for legacy launcher icons.
android = ROOT / "companion/apps/android/src/main/res"
adaptive = ET.parse(android / "mipmap-anydpi-v26/ic_launcher.xml").getroot()
inset = float(adaptive.find("foreground/inset").get("{http://schemas.android.com/apk/res/android}inset").removesuffix("%")) / 100
safe = ET.Element(f"{{{NS}}}svg", {"viewBox": "0 0 1080 1080"})
safe.set("width", "1080")
safe.set("height", "1080")
scaled = ET.SubElement(safe, f"{{{NS}}}g", {"transform": f"translate({1080 * inset:g} {1080 * inset:g}) scale({1 - 2 * inset:g})"})
scaled.append(deepcopy(foreground))
mono = deepcopy(foreground)
for element in mono.iter():
    if element.get("style", "").startswith("fill:") and element.get("style") != "fill:none;":
        element.set("style", "fill:white;")
legacy = deepcopy(background)
legacy.append(deepcopy(safe))
legacy.set("viewBox", "180 180 720 720")
# Web maskable icons have an 80% safe circle and no Android 108-to-72 crop.
maskable = deepcopy(background)
web_foreground = ET.SubElement(maskable, f"{{{NS}}}g", {"transform": "translate(129.6 129.6) scale(.76)"})
web_foreground.append(deepcopy(foreground))
# Fail regeneration if a future artwork change escapes the guaranteed mask.
for svg, diameter in [(safe, 72 / 108), (web_foreground, .8)]:
    canvas = ET.Element(f"{{{NS}}}svg", {"viewBox": "0 0 1080 1080"})
    canvas.append(deepcopy(svg))
    alpha = raster(canvas, 432).getchannel("A")
    # Compare pixel centres, allowing half a raster pixel for edge antialiasing.
    radius = 432 * diameter / 2
    assert all((x + .5 - 216) ** 2 + (y + .5 - 216) ** 2 <= (radius + .5) ** 2
               for y in range(432) for x in range(432) if alpha.getpixel((x, y)) > 127), \
        "Artwork escapes its icon circle"
for name, svg in [("full_converted.svg", icon), ("foreground.svg", foreground), ("background.svg", background), ("monochrome.svg", mono)]:
    write(STORE / name, serialize(svg))
for name, svg in [("icon.svg", icon), ("favicon.svg", icon), ("icon-maskable.svg", maskable)]:
    write(WEB / "resources" / name, serialize(svg))

lockup_svg = read("RSVP-Companion-logo-color.svg")
_, _, logo_width, logo_height = lockup_svg.get("viewBox").split()
lockup_svg.set("width", logo_width)
lockup_svg.set("height", logo_height)
lockup = serialize(lockup_svg)
write(STORE / "icon_horizontal.svg", lockup)
write(WEB / "composeResources/drawable/rsvp_nano_horizontal.svg", lockup)
write(WEB / "composeResources/drawable/rsvp_nano_horizontal_light.svg", lockup.replace("fill:white;", "fill:#191c1a;"))

for path in android.glob("*/ic_launcher*.png"):
    size = Image.open(path).width
    source = {"ic_launcher_foreground": foreground, "ic_launcher_background": background, "ic_launcher_monochrome": mono}.get(path.stem, legacy)
    output = raster(source, size)
    if path.stem == "ic_launcher_round":
        mask = Image.new("L", (size, size))
        ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
        output.putalpha(mask)
    output.save(path)
raster(icon, 512).convert("RGB").save(STORE / "android/icon-512.png")
ios = ROOT / "companion/apps/ios/RSVPNanoCompanion/RSVPNanoCompanion/Assets.xcassets/AppIcon.appiconset"
for item in json.loads((ios / "Contents.json").read_text())["images"]:
    size = round(float(item["size"].split("x")[0]) * float(item["scale"].removesuffix("x")))
    raster(icon, size).convert("RGB").save(ios / item["filename"])

# Reveal every glyph in place as the reading markers move across its center.
# Keep the supplied geometry, including the overlapping P/A outlines.
import re

logo = read("RSVP-Companion-logo-color.svg")
logo.tag = f"{{{NS}}}g"
for attribute in ("width", "height", "viewBox", "version"):
    logo.attrib.pop(attribute, None)
removed_markers = 0
for group in logo.iter(f"{{{NS}}}g"):
    for element in list(group):
        if element.tag == f"{{{NS}}}rect" and element.get("style") == "fill:rgb(255,0,0);":
            group.remove(element)
            removed_markers += 1
assert removed_markers == 2, "Supplied focus-marker geometry changed"
paths = list(logo.iter(f"{{{NS}}}path"))
symbol_path, c_path, word_path = paths[0], paths[-2], paths[-1]
parents = {child: parent for parent in logo.iter() for child in parent}
# Keep the RSVP rhythm, then accelerate through COMPANION after reaching C.
arrivals = [0, 120, 240, 360, 480, 540, 590, 630, 665, 695, 725, 750, 775]
reveal_ms = 300
duration_ms = arrivals[-1] + reveal_ms
# Subpath pairs keep each glyph's counters/holes with its outer outline.
for source, partitions, first in [
    (symbol_path, [(0, 2), (2, 3), (3, 4), (4, 6)], 0),
    (word_path, [(0, 2), (2, 3), (3, 5), (5, 7), (7, 8), (8, 9), (9, 11), (11, 12)], 5),
]:
    parts = re.findall(r"M[^M]*", source.get("d"))
    assert len(parts) == partitions[-1][1], "Supplied glyph geometry changed"
    parent = parents[source]
    position = list(parent).index(source)
    parent.remove(source)
    for index, (start, end) in enumerate(partitions):
        letter = deepcopy(source)
        letter.set("d", "".join(parts[start:end]))
        letter.set("class", "letter")
        letter.set("style", f"animation-delay:{arrivals[first + index]}ms")
        parent.insert(position + index, letter)
c_path.set("class", "letter")
c_path.set("style", "animation-delay:480ms;--settled:#ff0000;fill-rule:nonzero")
for element in logo.iter():
    if element.get("style") == "fill:white;":
        element.attrib.pop("style")
# Letter centers measured from the supplied SVG, in its 1350 x 350 viewBox.
centers = [88, 262, 88, 262, 410, 524.81, 651.69, 779.46, 880.56, 996.59, 1084.96, 1167.67, 1287.11]
focus_frames = []
for index, center in enumerate(centers):
    arrival = arrivals[index] / duration_ms * 100
    if index < 4:
        hold = (arrivals[index] + 45) / duration_ms * 100
        focus_frames.append(f"{arrival:.3f}%,{hold:.3f}% {{ transform:translateX({center - 410:.2f}px) }}")
    else:
        # No per-letter stops after C: retain velocity while intervals shorten.
        focus_frames.append(f"{arrival:.3f}% {{ transform:translateX({center - 410:.2f}px);animation-timing-function:linear }}")
focus_frames.append(f"100% {{ transform:translateX({centers[-1] - 410:.2f}px) }}")
for suffix, ink in [("", "#ffffff"), ("_light", "#191c1a")]:
    svg = f'''<svg xmlns="{NS}" viewBox="0 0 1350 350" fill="{ink}" style="--settled:{ink}">
<style>
.letter {{ animation: reveal {reveal_ms}ms cubic-bezier(.22,.61,.36,1) both; }}
.focus {{ fill:#ff2424; animation: focus {duration_ms}ms cubic-bezier(.4,0,.2,1) both; }}
@keyframes reveal {{ 0% {{ opacity:0;fill:#ff7770 }} 12% {{ opacity:1;fill:#ff7770 }} 100% {{ opacity:1;fill:var(--settled) }} }}
@keyframes focus {{ {''.join(focus_frames)} }}
@media (prefers-reduced-motion:reduce) {{ .letter,.focus {{ animation:none }} .letter {{ fill:var(--settled) }} }}
</style>
{serialize(logo)}
<g class="focus"><rect x="400" y="10" width="20" height="37"/><rect x="400" y="303" width="20" height="37"/></g>
</svg>'''
    ET.fromstring(svg)
    write(WEB / "resources" / f"splash{suffix}.svg", svg)
print("Regenerated website and native companion branding.")
