#!/usr/bin/env python3
"""Keep legacy OTA discovery below its 32 KiB GitHub metadata limit.

Publish OTA images individually and put full browser images in one ZIP. Existing
LCD firmware through v0.0.8 needs the two exact legacy OTA filenames below.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path

from export_web_firmware import FLASH_EXPORTS, OTA_EXPORTS

BROWSER_BUNDLE = "rsvp-nano-browser-installers.zip"
LEGACY_OTA_ALIASES = {
    "rsvp-nano-ota.bin": "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
    "rsvp-nano-rev2-ota.bin": "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin",
}


def published_assets(directory: Path) -> list[Path]:
    names = [entry["binary"] for entry in OTA_EXPORTS]
    names += list(LEGACY_OTA_ALIASES) + [BROWSER_BUNDLE, "SHA256SUMS.txt"]
    if (directory / "update-channel.json").exists():
        names.append("update-channel.json")
    return [directory / name for name in names]


def package_release(directory: Path) -> None:
    # Check every board pair before preparing a publishable package. An OTA alias
    # must never accidentally contain a merged image or another board's firmware.
    ota_by_env = {entry["env"]: entry["binary"] for entry in OTA_EXPORTS}
    for entry in FLASH_EXPORTS:
        merged = (directory / entry["binary"]).read_bytes()
        ota = (directory / ota_by_env[entry["env"]]).read_bytes()
        if not ota or ota[0] != 0xE9 or merged[0x10000:] != ota:
            raise SystemExit(f"Merged/OTA firmware mismatch: {entry['env']}")
    for alias, canonical in LEGACY_OTA_ALIASES.items():
        shutil.copyfile(directory / canonical, directory / alias)
    with zipfile.ZipFile(directory / BROWSER_BUNDLE, "w", zipfile.ZIP_DEFLATED) as archive:
        for entry in FLASH_EXPORTS:
            archive.write(directory / entry["binary"], entry["binary"])
    # Include checksums for ZIP members as well as the individually published
    # files, so both manual extraction and website staging can verify each board.
    paths = [directory / entry["binary"] for entry in FLASH_EXPORTS]
    paths += [path for path in published_assets(directory) if path.name != "SHA256SUMS.txt"]
    (directory / "SHA256SUMS.txt").write_text("".join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
        for path in sorted(paths)
    ))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=Path("build/firmware"))
    parser.add_argument("--print-assets", action="store_true")
    args = parser.parse_args()
    if args.print_assets:
        for path in published_assets(args.directory):
            if not path.is_file():
                raise SystemExit(f"Missing release asset: {path}")
            print(path)
    else:
        package_release(args.directory)


if __name__ == "__main__":
    main()
