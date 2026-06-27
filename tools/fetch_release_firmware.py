#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB_FIRMWARE_DIR = ROOT / "web" / "firmware"
MANIFEST_PATH = WEB_FIRMWARE_DIR / "manifest.json"
REV2_MANIFEST_PATH = WEB_FIRMWARE_DIR / "manifest-rev2.json"
DEFAULT_REPO = "ionutdecebal/rsvpnano"
DEFAULT_REQUIRED_ASSETS = (
    "rsvp-nano.bin",
    "rsvp-nano-ota.bin",
    "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
)
DEFAULT_OPTIONAL_ASSETS = (
    "rsvp-nano-rev2.bin",
    "rsvp-nano-rev2-ota.bin",
    "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin",
    "rsvp-nano-esp32-s3-touch-amoled-1.8.bin",
    "rsvp-nano-esp32-s3-touch-amoled-1.8-ota.bin",
    "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin",
    "rsvp-nano-esp32-s3-touch-amoled-1.8-v2-ota.bin",
    "rsvp-nano-esp32-s3-touch-amoled-2.16.bin",
    "rsvp-nano-esp32-s3-touch-amoled-2.16-ota.bin",
    "rsvp-nano-esp32-s3-touch-amoled-2.41.bin",
    "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin",
)


def default_manifest(name: str, binary: str, features: list[str]) -> dict:
    return {
        "name": name,
        "version": "dev",
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "features": features,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "improv": False,
                "parts": [
                    {
                        "path": binary,
                        "offset": 0,
                    }
                ],
            }
        ],
    }


COMMON_FEATURES = [
    "Books and articles library",
    "Device-hosted web companion",
    "RSS feed downloads",
    "USB SD-card transfer mode",
]

FLASH_MANIFESTS = {
    "rsvp-nano.bin": {
        "path": MANIFEST_PATH,
        "fallback": default_manifest("RSVP Nano", "rsvp-nano.bin", COMMON_FEATURES),
    },
    "rsvp-nano-rev2.bin": {
        "path": REV2_MANIFEST_PATH,
        "fallback": default_manifest(
            "RSVP Nano Rev2",
            "rsvp-nano-rev2.bin",
            COMMON_FEATURES + ["GPIO42 backlight profile"],
        ),
    },
    "rsvp-nano-esp32-s3-touch-amoled-1.8.bin": {
        "path": WEB_FIRMWARE_DIR / "manifest-esp32-s3-touch-amoled-1.8.json",
        "fallback": default_manifest(
            "RSVP Nano Touch AMOLED 1.8",
            "rsvp-nano-esp32-s3-touch-amoled-1.8.bin",
            COMMON_FEATURES + ["Touch AMOLED 1.8 profile"],
        ),
    },
    "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin": {
        "path": WEB_FIRMWARE_DIR / "manifest-esp32-s3-touch-amoled-1.8-v2.json",
        "fallback": default_manifest(
            "RSVP Nano Touch AMOLED 1.8 V2 Test",
            "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin",
            COMMON_FEATURES + ["Touch AMOLED 1.8 V2 test profile"],
        ),
    },
    "rsvp-nano-esp32-s3-touch-amoled-2.16.bin": {
        "path": WEB_FIRMWARE_DIR / "manifest-esp32-s3-touch-amoled-2.16.json",
        "fallback": default_manifest(
            "RSVP Nano Touch AMOLED 2.16",
            "rsvp-nano-esp32-s3-touch-amoled-2.16.bin",
            COMMON_FEATURES + ["Touch AMOLED 2.16 profile"],
        ),
    },
    "rsvp-nano-esp32-s3-touch-amoled-2.41.bin": {
        "path": WEB_FIRMWARE_DIR / "manifest-esp32-s3-touch-amoled-2.41.json",
        "fallback": default_manifest(
            "RSVP Nano Touch AMOLED 2.41",
            "rsvp-nano-esp32-s3-touch-amoled-2.41.bin",
            COMMON_FEATURES + ["Touch AMOLED 2.41 profile"],
        ),
    },
}


def github_headers() -> dict[str, str]:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "rsvp-nano-web-firmware-fetcher",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers=github_headers())
    try:
        with urllib.request.urlopen(request) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace").strip()
        detail = f": {body}" if body else ""
        raise SystemExit(f"GitHub API request failed with HTTP {exc.code}{detail}") from exc
    except urllib.error.URLError as exc:
        raise SystemExit(f"GitHub API request failed: {exc.reason}") from exc


def download_file(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers=github_headers())
    try:
        with urllib.request.urlopen(request) as response, destination.open("wb") as output:
            shutil.copyfileobj(response, output)
    except urllib.error.HTTPError as exc:
        raise SystemExit(f"Asset download failed with HTTP {exc.code}: {destination.name}") from exc
    except urllib.error.URLError as exc:
        raise SystemExit(f"Asset download failed for {destination.name}: {exc.reason}") from exc


def latest_release(repo: str) -> dict:
    return fetch_json(f"https://api.github.com/repos/{repo}/releases/latest")


def find_asset(release: dict, name: str, required: bool = True) -> dict | None:
    for asset in release.get("assets", []):
        if asset.get("name") == name:
            return asset
    if required:
        raise SystemExit(f"Latest release is missing required asset: {name}")
    return None


def load_manifest(path: Path, fallback: dict) -> dict:
    if not path.exists():
        return json.loads(json.dumps(fallback))
    return json.loads(path.read_text())


def write_manifests(version: str, available_flash_assets: set[str], prune_missing: bool) -> None:
    for asset_name, manifest_info in FLASH_MANIFESTS.items():
        path = manifest_info["path"]
        if asset_name in available_flash_assets:
            manifest = load_manifest(path, manifest_info["fallback"])
            manifest["version"] = version
            path.write_text(json.dumps(manifest, indent=2) + "\n")
        elif prune_missing and path.exists():
            path.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Populate web/firmware from the latest published GitHub Release."
    )
    parser.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_REPOSITORY", DEFAULT_REPO),
        help="GitHub repository in owner/name form.",
    )
    parser.add_argument(
        "--asset",
        action="append",
        dest="assets",
        help="Release asset to download. Repeat to request multiple assets.",
    )
    args = parser.parse_args()

    release = latest_release(args.repo)
    tag_name = str(release.get("tag_name", "")).strip()
    if not tag_name:
        raise SystemExit("Latest release is missing tag_name.")

    WEB_FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    if args.assets:
        requested_assets = tuple(args.assets)
        downloaded_assets: set[str] = set()
        for asset_name in requested_assets:
            asset = find_asset(release, asset_name)
            url = str(asset.get("browser_download_url", "")).strip()
            if not url:
                raise SystemExit(f"Release asset is missing browser_download_url: {asset_name}")
            destination = WEB_FIRMWARE_DIR / asset_name
            print(f"Downloading {asset_name} from {tag_name} -> {destination}")
            download_file(url, destination)
            downloaded_assets.add(asset_name)
    else:
        downloaded_assets = set()
        for asset_name in DEFAULT_REQUIRED_ASSETS:
            asset = find_asset(release, asset_name)
            url = str(asset.get("browser_download_url", "")).strip()
            if not url:
                raise SystemExit(f"Release asset is missing browser_download_url: {asset_name}")
            destination = WEB_FIRMWARE_DIR / asset_name
            print(f"Downloading {asset_name} from {tag_name} -> {destination}")
            download_file(url, destination)
            downloaded_assets.add(asset_name)

        for asset_name in DEFAULT_OPTIONAL_ASSETS:
            asset = find_asset(release, asset_name, required=False)
            if asset is None:
                print(f"Skipping optional release asset not present in {tag_name}: {asset_name}")
                continue
            url = str(asset.get("browser_download_url", "")).strip()
            if not url:
                print(f"Skipping optional release asset with no download URL: {asset_name}")
                continue
            destination = WEB_FIRMWARE_DIR / asset_name
            print(f"Downloading {asset_name} from {tag_name} -> {destination}")
            download_file(url, destination)
            downloaded_assets.add(asset_name)

    write_manifests(
        tag_name,
        {asset_name for asset_name in downloaded_assets if asset_name in FLASH_MANIFESTS},
        prune_missing=not args.assets,
    )
    print(f"Web firmware updated to release {tag_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
