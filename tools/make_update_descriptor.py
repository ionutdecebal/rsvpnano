#!/usr/bin/env python3
"""Describe a clean LCD Rev1 or Rev2 OTA candidate. Does not publish or move tags."""
from pathlib import Path
import argparse
import hashlib
import json
import re

BOARDS = {
    "lcd349-v1": ("waveshare_esp32s3_touch_lcd_3_49", "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin"),
    "lcd349-v2": ("waveshare_esp32s3_touch_lcd_3_49_rev2", "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin"),
}

def descriptor(image: Path, channel: str, tag: str, commit: str, board: str = "lcd349-v1") -> dict:
    if board not in BOARDS:
        raise ValueError("Unsupported board")
    board_id, asset = BOARDS[board]
    if channel not in ("classic", "new-ui") or not re.fullmatch(r"[A-Za-z0-9._-]+", tag):
        raise ValueError("Invalid channel or tag")
    if not re.fullmatch(r"[a-f0-9]{40}", commit):
        raise ValueError("Invalid commit")
    data = image.read_bytes()
    version = (tag + "+" + commit[:12]).encode()
    if not 65536 <= len(data) <= 8*1024*1024 or data[0] != 0xE9 or version + b"\0" not in data:
        raise ValueError("Image must be a clean, version-stamped application-only build")
    if asset.encode() + b"\0" not in data:
        raise ValueError("Image does not contain the selected board's OTA asset identity")
    return {"schema": 1, "releases": [{
        "channel": channel, "board": board_id,
        "tag": tag, "commit": commit, "asset": asset,
        "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
    }]}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True, action="append")
    parser.add_argument("--board", choices=BOARDS, action="append",
                        help="Repeat with --image in matching order; defaults to lcd349-v1 for one image")
    parser.add_argument("--channel", choices=["classic","new-ui"], required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    boards = args.board or ["lcd349-v1"]
    if len(boards) != len(args.image) or len(set(boards)) != len(boards):
        parser.error("Provide one unique --board per --image")
    entries = [descriptor(image,args.channel,args.tag,args.commit,board)["releases"][0]
               for image,board in zip(args.image,boards)]
    args.output.write_text(json.dumps({"schema":1,"releases":entries},indent=2)+"\n")
