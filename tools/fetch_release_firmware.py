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
DEFAULT_REPO = "ionutdecebal/rsvpnano"
DEFAULT_ASSETS = ("rsvp-nano.bin", "rsvp-nano-ota.bin")
DEFAULT_MANIFEST = {
    "name": "RSVP Nano",
    "version": "dev",
    "new_install_prompt_erase": True,
    "new_install_improv_wait_time": 0,
    "builds": [
        {
            "chipFamily": "ESP32-S3",
            "improv": False,
            "parts": [
                {
                    "path": "rsvp-nano.bin",
                    "offset": 0,
                }
            ],
        }
    ],
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


def find_asset(release: dict, name: str) -> dict:
    for asset in release.get("assets", []):
        if asset.get("name") == name:
            return asset
    raise SystemExit(f"Latest release is missing required asset: {name}")


def load_manifest() -> dict:
    if not MANIFEST_PATH.exists():
        return json.loads(json.dumps(DEFAULT_MANIFEST))
    return json.loads(MANIFEST_PATH.read_text())


def write_manifest(version: str) -> None:
    manifest = load_manifest()
    manifest["version"] = version
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n")


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

    requested_assets = tuple(args.assets) if args.assets else DEFAULT_ASSETS
    WEB_FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    for asset_name in requested_assets:
        asset = find_asset(release, asset_name)
        url = str(asset.get("browser_download_url", "")).strip()
        if not url:
            raise SystemExit(f"Release asset is missing browser_download_url: {asset_name}")
        destination = WEB_FIRMWARE_DIR / asset_name
        print(f"Downloading {asset_name} from {tag_name} -> {destination}")
        download_file(url, destination)

    write_manifest(tag_name)
    print(f"Web firmware updated to release {tag_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
