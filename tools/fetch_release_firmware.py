#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile
import urllib.error
import urllib.request
import urllib.parse
import zipfile
from pathlib import Path

from export_web_firmware import FLASH_EXPORTS, OTA_EXPORTS, write_release_metadata
from package_release import BROWSER_BUNDLE


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = ROOT / "build" / "firmware"
DEFAULT_REPO = "ionutdecebal/rsvpnano"
ASSET_FALLBACKS = {
    "rsvp-nano-esp32-s3-touch-lcd-3.49.bin": ("rsvp-nano.bin",),
    "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2.bin": ("rsvp-nano-rev2.bin",),
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

def stage_ui_preview(repo: str, tag: str) -> dict:
    """Stage one explicit, immutable preview without replacing Classic files."""
    if not re.fullmatch(r"(?:preview-ui-dictionary-[A-Za-z0-9._-]+|v0\.2\.\d+(?:-[A-Za-z0-9._-]+)?)", tag):
        raise SystemExit("New UI requires an explicit preview or v0.2.x release tag.")
    release = fetch_json(f"https://api.github.com/repos/{repo}/releases/tags/{urllib.parse.quote(tag, safe='')}")
    if release.get("tag_name") != tag or not release.get("prerelease") or release.get("draft"):
        raise SystemExit("New UI must be a published prerelease at the selected tag.")
    name = next(x["binary"] for x in FLASH_EXPORTS if x["id"] == "lcd349-v1")
    asset = find_asset(release, name)
    sums = find_asset(release, "SHA256SUMS.txt")
    checksum_file = FIRMWARE_DIR / "new-ui-SHA256SUMS.txt"
    download_file(sums["browser_download_url"], checksum_file)
    expected = {}
    for line in checksum_file.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and re.fullmatch(r"[a-fA-F0-9]{64}", parts[0]):
            expected[parts[1].lstrip("*")] = parts[0].lower()
    destination = FIRMWARE_DIR / ("new-ui-" + name)
    pending = destination.with_suffix(".partial")
    try:
        download_file(asset["browser_download_url"], pending)
        if hashlib.sha256(pending.read_bytes()).hexdigest() != expected.get(name):
            raise SystemExit("New UI image checksum mismatch or missing checksum.")
        pending.replace(destination)
    finally:
        pending.unlink(missing_ok=True)
    return {"version": tag, "firmware": {"lcd349-v1": destination.name}}


def find_asset(release: dict, name: str, required: bool = True) -> dict | None:
    for asset in release.get("assets", []):
        if asset.get("name") == name:
            return asset
    if required:
        raise SystemExit(f"Latest release is missing required asset: {name}")
    return None


def find_asset_with_fallback(release: dict, name: str, required: bool = True) -> tuple[dict | None, str | None]:
    asset = find_asset(release, name, required=False)
    if asset is not None:
        return asset, name

    for fallback in ASSET_FALLBACKS.get(name, ()):
        asset = find_asset(release, fallback, required=False)
        if asset is not None:
            return asset, fallback

    if required:
        raise SystemExit(f"Latest release is missing required asset: {name}")
    return None, None


def stage_browser_bundle(release: dict, names: list[str]) -> set[str]:
    """Read verified, explicitly named ZIP members; never extract arbitrary paths."""
    missing = [name for name in names if find_asset_with_fallback(release, name, required=False)[0] is None]
    bundle = find_asset(release, BROWSER_BUNDLE, required=False)
    if not missing or bundle is None:
        return set()
    sums = find_asset(release, "SHA256SUMS.txt")
    with tempfile.TemporaryDirectory(dir=FIRMWARE_DIR) as temporary:
        root = Path(temporary)
        checksum_file = root / "SHA256SUMS.txt"
        download_file(sums["browser_download_url"], checksum_file)
        expected = {}
        for line in checksum_file.read_text().splitlines():
            parts = line.split()
            if len(parts) == 2 and re.fullmatch(r"[a-fA-F0-9]{64}", parts[0]):
                name = parts[1].lstrip("*")
                if name in expected:
                    raise SystemExit("Duplicate browser bundle checksum")
                expected[name] = parts[0].lower()
        archive_path = root / BROWSER_BUNDLE
        download_file(bundle["browser_download_url"], archive_path)
        digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
        if digest != expected.get(BROWSER_BUNDLE):
            raise SystemExit("Browser bundle checksum mismatch or missing checksum")
        if bundle.get("digest") and bundle["digest"] != "sha256:" + digest:
            raise SystemExit("Browser bundle GitHub digest mismatch")
        with zipfile.ZipFile(archive_path) as archive:
            members = archive.namelist()
            for name in missing:
                if members.count(name) != 1:
                    raise SystemExit(f"Missing or duplicate browser bundle member: {name}")
                if not 65536 < archive.getinfo(name).file_size <= 16 * 1024 * 1024:
                    raise SystemExit(f"Invalid browser image size: {name}")
                data = archive.read(name)
                if data[0] != 0xE9 or hashlib.sha256(data).hexdigest() != expected.get(name):
                    raise SystemExit(f"Browser image checksum mismatch: {name}")
                (root / name).write_bytes(data)
        # Do not replace any staged board until every requested image verifies.
        for name in missing:
            (root / name).replace(FIRMWARE_DIR / name)
    return set(missing)


def stage_update_catalog(repo: str, classic: dict, preview_tag: str) -> None:
    """Expose only releases containing a verified selector-compatible descriptor."""
    entries = []
    releases = [("classic", classic)]
    if preview_tag:
        releases.append(("new-ui", fetch_json(
            f"https://api.github.com/repos/{repo}/releases/tags/{urllib.parse.quote(preview_tag, safe='')}")))
    for channel, release in releases:
        descriptor = find_asset(release, "update-channel.json", required=False)
        if descriptor is None:
            # Older Classic has no selector; do not make it the round-trip target.
            continue
        if release.get("draft") or bool(release.get("prerelease")) != (channel == "new-ui"):
            raise SystemExit("Wrong release channel for update catalog")
        path = FIRMWARE_DIR / f"{channel}-update-channel.json"
        download_file(descriptor["browser_download_url"], path)
        document = json.loads(path.read_text())
        if document.get("schema") != 1 or len(document.get("releases", [])) != 1:
            raise SystemExit("Invalid update descriptor")
        entry = document["releases"][0]
        name = "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin"
        if (entry.get("channel") != channel or entry.get("tag") != release["tag_name"]
            or entry.get("board") != "waveshare_esp32s3_touch_lcd_3_49"
            or entry.get("asset") != name
            or not re.fullmatch(r"[a-f0-9]{64}", entry.get("sha256", ""))
            or not re.fullmatch(r"[a-f0-9]{40}", entry.get("commit", ""))
            or not 65536 <= entry.get("bytes", 0) <= 8 * 1024 * 1024):
            raise SystemExit("Invalid update entry")
        asset = find_asset(release, name)
        pending = FIRMWARE_DIR / f"{channel}-ota.partial"
        try:
            download_file(asset["browser_download_url"], pending)
            data = pending.read_bytes()
            version = (entry["tag"] + "+" + entry["commit"][:12]).encode()
            if (len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]
                or data[0] != 0xE9 or version + b"\0" not in data):
                raise SystemExit("OTA image does not match its update descriptor")
        finally:
            pending.unlink(missing_ok=True)
        entries.append(entry)
    pending = FIRMWARE_DIR / "channels.json.partial"
    pending.write_text(json.dumps({"schema": 1, "releases": entries}, indent=2) + "\n")
    pending.replace(FIRMWARE_DIR / "channels.json")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Populate build/firmware from the latest published GitHub Release."
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
    parser.add_argument("--preview-tag", default=os.environ.get("RSVP_UI_PREVIEW_TAG", "").strip(),
                        help="Explicit accepted new-UI prerelease tag; Classic remains the default.")
    args = parser.parse_args()

    release = latest_release(args.repo)
    tag_name = str(release.get("tag_name", "")).strip()
    if not tag_name:
        raise SystemExit("Latest release is missing tag_name.")

    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    requested_assets = tuple(args.assets) if args.assets else None
    available_firmware: dict[str, str] = {}
    all_exports = (*FLASH_EXPORTS, *OTA_EXPORTS)
    exports = all_exports if requested_assets is not None else FLASH_EXPORTS
    bundled = stage_browser_bundle(release, [entry["binary"] for entry in FLASH_EXPORTS
        if requested_assets is None or entry["binary"] in requested_assets])
    for export in exports:
        asset_name = export["binary"]
        if requested_assets is not None and asset_name not in requested_assets:
            continue
        if asset_name in bundled:
            available_firmware[export["id"]] = asset_name
            print(f"Verified {asset_name} from {tag_name} browser bundle")
            continue
        asset, release_asset_name = find_asset_with_fallback(
            release,
            asset_name,
            required=requested_assets is not None,
        )
        if asset is None:
            print(f"Skipping release asset not present in {tag_name}: {asset_name}")
            continue
        url = str(asset.get("browser_download_url", "")).strip()
        if not url:
            if requested_assets is not None:
                raise SystemExit(f"Release asset is missing browser_download_url: {asset_name}")
            print(f"Skipping release asset with no download URL: {asset_name}")
            continue
        destination = FIRMWARE_DIR / asset_name
        print(f"Downloading {release_asset_name} from {tag_name} -> {destination}")
        download_file(url, destination)
        if "id" in export:
            available_firmware[export["id"]] = asset_name

    if requested_assets is not None:
        unknown_assets = set(requested_assets) - {export["binary"] for export in all_exports}
        if unknown_assets:
            formatted = ", ".join(sorted(unknown_assets))
            raise SystemExit(f"Unknown firmware asset requested: {formatted}")
    if requested_assets is None and not available_firmware:
        raise SystemExit(f"Release {tag_name} has no browser-flasher images.")

    if available_firmware:
        write_release_metadata(tag_name, available_firmware)
        if args.preview_tag:
            metadata_path = FIRMWARE_DIR / "release.json"
            metadata = json.loads(metadata_path.read_text())
            metadata["newUi"] = stage_ui_preview(args.repo, args.preview_tag)
            metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
        stage_update_catalog(args.repo, release, args.preview_tag)
    print(f"Web firmware updated to release {tag_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
