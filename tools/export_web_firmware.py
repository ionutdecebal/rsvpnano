#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB_FIRMWARE_DIR = ROOT / "web" / "firmware"
RELEASE_METADATA_PATH = WEB_FIRMWARE_DIR / "release.json"
VERSION_DIR = WEB_FIRMWARE_DIR / "versions"
BOOT_APP0_GLOB = "framework-arduinoespressif32*/tools/partitions/boot_app0.bin"
VERSION_DECLARATION = "inline constexpr char kFirmwareVersion[] = "

FLASH_EXPORTS = (
    {
        "id": "lcd349-v1",
        "env": "waveshare_esp32s3_touch_lcd_349_rev1",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49.bin",
        "label": "RSVP Nano Touch LCD 3.49 rev1 firmware",
    },
    {
        "id": "lcd349-v2",
        "env": "waveshare_esp32s3_touch_lcd_349_rev2",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2.bin",
        "label": "RSVP Nano Touch LCD 3.49 rev2 firmware",
    },
    {
        "id": "amoled18-v1",
        "env": "waveshare_esp32s3_touch_amoled_18_v1",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 v1 firmware",
    },
    {
        "id": "amoled18-v2",
        "env": "waveshare_esp32s3_touch_amoled_18_v2",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 v2 firmware",
    },
    {
        "id": "amoled206",
        "env": "waveshare_esp32s3_touch_amoled_206",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.06.bin",
        "label": "RSVP Nano Touch AMOLED 2.06 firmware",
    },
    {
        "id": "amoled216",
        "env": "waveshare_esp32s3_touch_amoled_216",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.16.bin",
        "label": "RSVP Nano Touch AMOLED 2.16 firmware",
    },
    {
        "id": "amoled241",
        "env": "waveshare_esp32s3_touch_amoled_241",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.41.bin",
        "label": "RSVP Nano Touch AMOLED 2.41 firmware",
    },
    {
        "id": "lcd147-c6",
        "env": "waveshare_esp32c6_touch_lcd_147",
        "binary": "rsvp-nano-esp32-c6-touch-lcd-1.47.bin",
        "label": "RSVP Nano ESP32-C6 Touch LCD 1.47 firmware",
    },
)

OTA_EXPORTS = (
    {
        "env": "waveshare_esp32s3_touch_lcd_349_rev1",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_lcd_349_rev2",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 rev2 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18_v1",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-ota.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 v1 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18_v2",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-v2-ota.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 v2 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_206",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.06-ota.bin",
        "label": "RSVP Nano Touch AMOLED 2.06 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_216",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.16-ota.bin",
        "label": "RSVP Nano Touch AMOLED 2.16 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_241",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin",
        "label": "RSVP Nano Touch AMOLED 2.41 OTA firmware",
    },
    {
        "env": "waveshare_esp32c6_touch_lcd_147",
        "binary": "rsvp-nano-esp32-c6-touch-lcd-1.47-ota.bin",
        "label": "RSVP Nano ESP32-C6 Touch LCD 1.47 OTA firmware",
    },
)

REQUIRED_ENVS = tuple(
    sorted({export["env"] for export in FLASH_EXPORTS} | {export["env"] for export in OTA_EXPORTS})
)


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


def read_generated_version(env: str) -> str:
    header = ROOT / ".pio" / "build" / env / "generated" / "FirmwareVersion.generated.h"
    if not header.exists():
        raise SystemExit(f"Missing generated firmware version header for {env}: {header}")

    for line in header.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith(VERSION_DECLARATION) or not stripped.endswith(";"):
            continue

        encoded_version = stripped[len(VERSION_DECLARATION) : -1].strip()
        version = json.loads(encoded_version)
        if isinstance(version, str) and version:
            return version

    raise SystemExit(f"Could not read firmware version from generated header: {header}")


def generated_version(envs: list[str]) -> str:
    versions = {read_generated_version(env) for env in envs}
    if len(versions) != 1:
        formatted = ", ".join(sorted(versions))
        raise SystemExit(f"PlatformIO environments produced different firmware versions: {formatted}")

    return versions.pop()


def write_release_metadata(version: str, firmware: dict[str, str]) -> None:
    metadata = {"version": version, "firmware": firmware}
    RELEASE_METADATA_PATH.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


def write_version_marker(env: str, version: str) -> None:
    VERSION_DIR.mkdir(parents=True, exist_ok=True)
    (VERSION_DIR / f"{env}.txt").write_text(version + "\n", encoding="utf-8")


def assemble_release() -> None:
    missing_markers = [env for env in REQUIRED_ENVS if not (VERSION_DIR / f"{env}.txt").exists()]
    if missing_markers:
        raise SystemExit(f"Missing firmware version markers: {', '.join(missing_markers)}")

    versions = {
        (VERSION_DIR / f"{env}.txt").read_text(encoding="utf-8").strip()
        for env in REQUIRED_ENVS
    }
    if len(versions) != 1 or not next(iter(versions)):
        raise SystemExit(f"Firmware jobs produced different versions: {', '.join(sorted(versions))}")

    for export in (*FLASH_EXPORTS, *OTA_EXPORTS):
        path = WEB_FIRMWARE_DIR / export["binary"]
        if not path.exists():
            raise SystemExit(f"Missing exported firmware: {path}")

    version = versions.pop()
    write_release_metadata(version, {export["id"]: export["binary"] for export in FLASH_EXPORTS})
    shutil.rmtree(VERSION_DIR)
    print(f"Firmware release assembled in {WEB_FIRMWARE_DIR}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build merged binaries for the web flasher.")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Use existing .pio build outputs instead of running PlatformIO first.",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--env", choices=REQUIRED_ENVS, help="Build and export one PlatformIO environment.")
    mode.add_argument("--list-envs", action="store_true", help="Print the build environment matrix as JSON.")
    mode.add_argument("--assemble", action="store_true", help="Assemble artifacts exported by --env jobs.")
    args = parser.parse_args()

    if args.list_envs:
        print(json.dumps(REQUIRED_ENVS))
        return 0
    if args.assemble:
        assemble_release()
        return 0

    required_envs = [args.env] if args.env else list(REQUIRED_ENVS)

    if not args.skip_build:
        pio = pio_command()
        for env in required_envs:
            run([pio, "run", "-e", env])

    version = generated_version(required_envs)
    print(f"Firmware version: {version}")

    WEB_FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    for export in FLASH_EXPORTS:
        if export["env"] not in required_envs:
            continue
        output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {output}")
        merge_firmware(export["env"], output)

    for export in OTA_EXPORTS:
        if export["env"] not in required_envs:
            continue
        ota_output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {ota_output}")
        export_ota_binary(export["env"], ota_output)

    if args.env:
        write_version_marker(args.env, version)
    else:
        write_release_metadata(version, {export["id"]: export["binary"] for export in FLASH_EXPORTS})
        shutil.rmtree(VERSION_DIR, ignore_errors=True)

    print(f"Web firmware exported to {WEB_FIRMWARE_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
