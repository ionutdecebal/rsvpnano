#!/usr/bin/env python3
"""Describe a tested LCD Rev1 OTA image. Does not publish or move tags."""
from pathlib import Path
import argparse
import hashlib
import json
import re

def descriptor(image: Path, channel: str, tag: str, commit: str) -> dict:
    if channel not in ("classic", "new-ui") or not re.fullmatch(r"[A-Za-z0-9._-]+", tag):
        raise ValueError("Invalid channel or tag")
    if not re.fullmatch(r"[a-f0-9]{40}", commit):
        raise ValueError("Invalid commit")
    data = image.read_bytes()
    version = (tag + "+" + commit[:12]).encode()
    if not 65536 <= len(data) <= 8*1024*1024 or data[0] != 0xE9 or version + b"\0" not in data:
        raise ValueError("Image must be a clean, version-stamped application-only build")
    return {"schema": 1, "releases": [{
        "channel": channel, "board": "waveshare_esp32s3_touch_lcd_3_49",
        "tag": tag, "commit": commit, "asset": "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
        "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
    }]}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--channel", choices=["classic","new-ui"], required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(descriptor(args.image,args.channel,args.tag,args.commit),indent=2)+"\n")
