#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB_FIRMWARE_DIR = ROOT / "web" / "firmware"

EXPORTS = {
    "waveshare_esp32s3": {
        "binary": "rsvp-nano-v2.bin",
        "manifest": "manifest.json",
        "label": "Reader firmware",
    },
    "waveshare_esp32s3_usb_msc": {
        "binary": "rsvp-nano-v2-usb-msc.bin",
        "manifest": "manifest-usb-msc.json",
        "label": "USB transfer firmware",
    },
}


def run(command: list[str]) -> None:
    print("+", " ".join(command))
    env = os.environ.copy()
    env.setdefault("PLATFORMIO_SETTING_ENABLE_TELEMETRY", "No")
    subprocess.run(command, cwd=ROOT, check=True, env=env)


def pio_command() -> str:
    local = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if local.exists():
        return str(local)

    found = shutil.which("pio")
    if found:
        return found

    raise SystemExit("PlatformIO Core was not found. Install it or activate the PlatformIO env.")


def git_version() -> str:
    try:
        value = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
        ).strip()
        return value or "dev"
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def load_flash_parts(env: str) -> list[tuple[int, Path]]:
    idedata_path = ROOT / ".pio" / "build" / env / "idedata.json"
    if not idedata_path.exists():
        raise SystemExit(f"Missing {idedata_path}. Run the PlatformIO build first.")

    data = json.loads(idedata_path.read_text())
    extra = data.get("extra", {})
    parts: list[tuple[int, Path]] = []

    for image in extra.get("flash_images", []):
        parts.append((int(image["offset"], 16), Path(image["path"])))

    app_offset = int(extra.get("application_offset", "0x10000"), 16)
    parts.append((app_offset, ROOT / ".pio" / "build" / env / "firmware.bin"))

    for _, path in parts:
        if not path.exists():
            raise SystemExit(f"Missing flash part for {env}: {path}")

    return sorted(parts, key=lambda item: item[0])


def merge_firmware(pio: str, env: str, output: Path) -> None:
    parts = load_flash_parts(env)
    merge_args: list[str] = [
        pio,
        "pkg",
        "exec",
        "-p",
        "tool-esptoolpy",
        "--",
        "esptool.py",
        "--chip",
        "esp32s3",
        "merge_bin",
        "-o",
        str(output),
        "--flash_mode",
        "dio",
        "--flash_freq",
        "80m",
        "--flash_size",
        "16MB",
    ]

    for offset, path in parts:
        merge_args.extend([hex(offset), str(path)])

    run(merge_args)


def update_manifest(path: Path, version: str) -> None:
    manifest = json.loads(path.read_text())
    manifest["version"] = version
    path.write_text(json.dumps(manifest, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build merged binaries for the web flasher.")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Use existing .pio build outputs instead of running PlatformIO first.",
    )
    parser.add_argument("--version", default=git_version(), help="Version string for manifests.")
    args = parser.parse_args()

    pio = pio_command()
    WEB_FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    for env, export in EXPORTS.items():
        if not args.skip_build:
            run([pio, "run", "-e", env])

        output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {output}")
        merge_firmware(pio, env, output)
        update_manifest(WEB_FIRMWARE_DIR / export["manifest"], args.version)

    print(f"Web firmware exported to {WEB_FIRMWARE_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
