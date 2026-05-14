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
BOOT_APP0_GLOB = "framework-arduinoespressif32*/tools/partitions/boot_app0.bin"

EXPORTS = {
    "waveshare_esp32s3_usb_msc": {
        "binary": "rsvp-nano.bin",
        "ota_binary": "rsvp-nano-ota.bin",
        "manifest": "manifest.json",
        "label": "RSVP Nano firmware",
    },
}


def run(command: list[str], version: str | None = None) -> None:
    print("+", " ".join(command))
    env = os.environ.copy()
    env.setdefault("PLATFORMIO_SETTING_ENABLE_TELEMETRY", "No")
    if version:
        env["RSVP_FIRMWARE_VERSION"] = version
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
            ["git", "describe", "--tags", "--always", "--dirty"], cwd=ROOT, text=True
        ).strip()
        return value or "dev"
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def find_boot_app0() -> Path:
    search_roots = [
        ROOT / ".pio" / "packages",
        Path.home() / ".platformio" / "packages",
    ]
    candidates: list[Path] = []
    for root in search_roots:
        candidates.extend(root.glob(BOOT_APP0_GLOB))

    if not candidates:
        raise SystemExit("Could not find Arduino ESP32 boot_app0.bin after PlatformIO build.")

    return sorted(candidates)[-1]


def load_flash_parts(env: str) -> list[tuple[int, Path]]:
    build_dir = ROOT / ".pio" / "build" / env
    parts: list[tuple[int, Path]] = [
        (0x0000, build_dir / "bootloader.bin"),
        (0x8000, build_dir / "partitions.bin"),
        (0xE000, find_boot_app0()),
        (0x10000, build_dir / "firmware.bin"),
    ]

    for _, path in parts:
        if not path.exists():
            raise SystemExit(f"Missing flash part for {env}: {path}")

    return sorted(parts, key=lambda item: item[0])


def merge_firmware(env: str, output: Path) -> None:
    parts = load_flash_parts(env)
    output.parent.mkdir(parents=True, exist_ok=True)

    cursor = 0
    with output.open("wb") as merged:
        for offset, path in parts:
            if offset < cursor:
                raise SystemExit(f"Overlapping flash part for {env}: {path}")

            gap = offset - cursor
            if gap > 0:
                merged.write(b"\xFF" * gap)
                cursor = offset

            data = path.read_bytes()
            merged.write(data)
            cursor += len(data)


def export_ota_binary(env: str, output: Path) -> None:
    firmware_path = ROOT / ".pio" / "build" / env / "firmware.bin"
    if not firmware_path.exists():
        raise SystemExit(f"Missing OTA app binary for {env}: {firmware_path}")

    shutil.copy2(firmware_path, output)


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
            run([pio, "run", "-e", env], args.version)

        output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {output}")
        merge_firmware(env, output)
        ota_output = WEB_FIRMWARE_DIR / export["ota_binary"]
        print(f"Exporting OTA app -> {ota_output}")
        export_ota_binary(env, ota_output)
        update_manifest(WEB_FIRMWARE_DIR / export["manifest"], args.version)

    print(f"Web firmware exported to {WEB_FIRMWARE_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
