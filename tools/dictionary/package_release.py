#!/usr/bin/env python3
"""Prepare verified English dictionary release assets; does not publish them."""
import argparse
import hashlib
import json
import pathlib
import shutil
import zipfile
from build_dictionary import ATTRIBUTION

HERE = pathlib.Path(__file__).resolve().parent


def package(source, output):
    spec = json.loads((HERE / "english-2025.1.json").read_text())
    data = (source / "english.oewn").read_bytes()
    manifest = json.loads((source / "manifest.json").read_text())
    if (len(data) != spec["bytes"] or hashlib.sha256(data).hexdigest() != spec["sha256"]
            or data[:8] != b"RSVPDIC1"):
        raise ValueError("Dictionary does not match the pinned English pack")
    for field in ("format", "language", "source", "sourceSha256", "bytes", "sha256"):
        if manifest.get(field) != spec[field]:
            raise ValueError(f"Source manifest mismatch: {field}")
    if not 0 < len(ATTRIBUTION.encode()) <= 4096:
        raise ValueError("Attribution must fit the device's verified download bound")
    output.mkdir(parents=True, exist_ok=False)
    shutil.copy2(source / "english.oewn", output / "english.oewn")
    manifest.update(packVersion=spec["tag"], sourceUrl=spec["sourceUrl"], attribution=ATTRIBUTION)
    encoded = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode()
    if len(encoded) > 4096:
        raise ValueError("Manifest exceeds the device's download bound")
    (output / "manifest.json").write_bytes(encoded)
    (output / "ATTRIBUTION.txt").write_text(ATTRIBUTION + "\n")
    for name in ("OEWN-2025-LICENSE.md", "WNDB-LICENSE.txt"):
        shutil.copy2(HERE / "licenses" / name, output / name)
    (output / "README.md").write_text("""# RSVP Nano English dictionary · Open English Wordnet 2025

English definitions for the RSVP Nano New UI. More languages are planned.
This is a dictionary data release, versioned separately from firmware.

On a compatible reader, open Dictionary → ? → Install / manage dictionary.
Connect to Wi-Fi and install the English pack. It uses about 48 MiB on SD.

For manual installation, extract RSVP-Nano-English-Dictionary.zip and copy
its dictionaries folder to the root of the SD card. Keep the attribution and
licences with the pack. Existing compatible English packs remain usable.

The raw english.oewn, manifest.json and ATTRIBUTION.txt assets are for direct
device installation. The ZIP is for manual SD copying. SHA256SUMS.txt covers
all supplied assets. The indexed pack has 131,094 keys including word forms;
each entry contains up to eight senses and is bounded to 6 KiB.

Source: https://en-word.net/static/english-wordnet-2025-json.zip
Licence: CC BY 4.0, with underlying Princeton WordNet notices preserved.
See ATTRIBUTION.txt, OEWN-2025-LICENSE.md and WNDB-LICENSE.txt.

No firmware images are included. Classic firmware remains a separate release.
""")
    with zipfile.ZipFile(output / "RSVP-Nano-English-Dictionary.zip", "w",
                         compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name in ("english.oewn", "manifest.json", "ATTRIBUTION.txt"):
            archive.write(output / name, "dictionaries/" + name)
        for name in ("OEWN-2025-LICENSE.md", "WNDB-LICENSE.txt"):
            archive.write(output / name, "dictionaries/licenses/" + name)
    files = sorted(output.iterdir())
    (output / "SHA256SUMS.txt").write_text("".join(
        hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n"
        for path in files))
    print(f"{spec['tag']}: verified release assets prepared in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path, help="Existing verified pack directory")
    parser.add_argument("output", type=pathlib.Path, help="New release-assets directory")
    args = parser.parse_args()
    package(args.source, args.output)
