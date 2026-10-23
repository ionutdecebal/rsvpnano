#!/usr/bin/env python3
"""Reject a release whose OTA aliases cannot be discovered by v0.0.1–v0.0.8."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from package_release import LEGACY_OTA_ALIASES

LEGACY_READ_LIMIT = 32768
JSON_STRING = r'"(?:[^"\\]|\\.)*"'


def check_metadata(body: bytes) -> None:
    document = json.loads(body)
    prefix = body[:LEGACY_READ_LIMIT].decode("utf-8", errors="replace")
    tag = re.search(r'"tag_name"\s*:\s*(' + JSON_STRING + ')', prefix)
    if tag is None or json.loads(tag[1]) != document["tag_name"]:
        raise SystemExit("Release tag is outside the legacy metadata limit")
    assets = {asset["name"]: asset for asset in document["assets"]}
    for alias, canonical in LEGACY_OTA_ALIASES.items():
        if alias not in assets or canonical not in assets:
            raise SystemExit(f"Missing legacy OTA alias or canonical image: {alias}")
        old, current = assets[alias], assets[canonical]
        if (not current.get("digest", "").startswith("sha256:")
                or old.get("digest") != current["digest"] or old.get("size") != current["size"]):
            raise SystemExit(f"Legacy OTA alias does not match its board image: {alias}")
        found = None
        # The legacy parser scans string-valued name fields, then the following
        # browser_download_url. It can work with truncated JSON, but only if that
        # entire URL has arrived within the first 32768 response bytes.
        for match in re.finditer(r'"name"\s*:\s*(' + JSON_STRING + ')', prefix):
            if json.loads(match[1]) == alias:
                url = re.search(r'"browser_download_url"\s*:\s*(' + JSON_STRING + ')', prefix[match.end():])
                if url:
                    found = json.loads(url[1])
                break
        if not found or found != old.get("browser_download_url"):
            raise SystemExit(f"Legacy OTA URL is outside the first {LEGACY_READ_LIMIT} bytes: {alias}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("response", type=Path)
    args = parser.parse_args()
    body = args.response.read_bytes()
    check_metadata(body)
    print(f"Legacy OTA discovery verified for both LCD revisions ({len(body)} metadata bytes).")
