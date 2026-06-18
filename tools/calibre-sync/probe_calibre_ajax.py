#!/usr/bin/env python3
"""
probe_calibre_ajax.py — Host-side prototype for calibre-server AJAX endpoints.

Walks: library-info → search → book metadata → resolve rsvp URL + size + mtime.
Captures raw JSON fixtures for firmware unit tests.

Usage:
  python3 probe_calibre_ajax.py --base-url http://localhost:8099
  python3 probe_calibre_ajax.py --base-url http://192.168.1.10:8081 --library mylib --query rsvp
  python3 probe_calibre_ajax.py --base-url http://localhost:8099 --save-fixtures ./fixtures
"""

import argparse
import json
import sys
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path


def fetch_json(url: str) -> dict:
    """Fetch a URL and parse JSON. Raises on HTTP errors."""
    try:
        with urllib.request.urlopen(url, timeout=10) as resp:
            raw = resp.read()
            return json.loads(raw)
    except urllib.error.HTTPError as e:
        print(f"  HTTP {e.code} {e.reason} — {url}", file=sys.stderr)
        raise
    except urllib.error.URLError as e:
        print(f"  Connection error: {e.reason} — {url}", file=sys.stderr)
        raise


def save_fixture(directory: Path, name: str, data: dict) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    out = directory / name
    out.write_text(json.dumps(data, indent=4))
    print(f"  Saved fixture: {out}")


def probe(base_url: str, library_id: str | None, query: str, fixtures_dir: Path | None) -> None:
    base_url = base_url.rstrip("/")

    # ── Step 1: library-info ────────────────────────────────────────────────
    print("\n[1] GET /ajax/library-info")
    lib_info_url = f"{base_url}/ajax/library-info"
    lib_info = fetch_json(lib_info_url)
    default_lib = lib_info.get("default_library", "")
    library_map = lib_info.get("library_map", {})
    print(f"    default_library : {default_lib!r}")
    print(f"    library_map     : {library_map}")

    if fixtures_dir:
        save_fixture(fixtures_dir, "library-info.json", lib_info)

    # Resolve which library to use
    lib = library_id if library_id else default_lib
    if lib not in library_map:
        print(f"  WARNING: library {lib!r} not in library_map; using anyway", file=sys.stderr)
    print(f"    Using library   : {lib!r}")

    # ── Step 2: search ──────────────────────────────────────────────────────
    print(f"\n[2] GET /ajax/search?query={query!r}&library_id={lib!r}")
    search_params = urllib.parse.urlencode({"query": query, "library_id": lib})
    search_url = f"{base_url}/ajax/search?{search_params}"
    search_data = fetch_json(search_url)
    book_ids = search_data.get("book_ids", [])
    total_num = search_data.get("total_num", 0)
    num = search_data.get("num", 0)
    offset = search_data.get("offset", 0)
    print(f"    total_num : {total_num}")
    print(f"    num       : {num}  (page size returned)")
    print(f"    offset    : {offset}")
    print(f"    book_ids  : {book_ids}")

    if fixtures_dir:
        save_fixture(fixtures_dir, "search.json", search_data)

    if not book_ids:
        print(f"\n  No books found for query={query!r}. Done.")
        return

    # ── Step 3: book metadata ───────────────────────────────────────────────
    for book_id in book_ids:
        print(f"\n[3] GET /ajax/book/{book_id}?library_id={lib!r}")
        book_url = f"{base_url}/ajax/book/{book_id}?library_id={lib}"
        book_data = fetch_json(book_url)

        title = book_data.get("title", "<unknown>")
        authors = book_data.get("authors", [])
        last_modified = book_data.get("last_modified", "<missing>")
        formats = book_data.get("formats", [])
        format_metadata = book_data.get("format_metadata", {})
        other_formats = book_data.get("other_formats", {})

        print(f"    title         : {title!r}")
        print(f"    authors       : {authors}")
        print(f"    last_modified : {last_modified}")
        print(f"    formats       : {formats}")

        if fixtures_dir:
            save_fixture(fixtures_dir, f"book-{book_id}.json", book_data)

        # ── Step 4: resolve rsvp ───────────────────────────────────────────
        rsvp_path = other_formats.get("rsvp")
        rsvp_meta = format_metadata.get("rsvp", {})

        if not rsvp_path:
            print(f"\n  [!] Book {book_id} has no 'rsvp' in other_formats — skip download.")
            print(f"      available formats: {list(other_formats.keys())}")
            continue

        rsvp_url = f"{base_url}{rsvp_path}"
        rsvp_size = rsvp_meta.get("size", "<missing>")
        rsvp_mtime = rsvp_meta.get("mtime", "<missing>")

        print(f"\n[4] RSVP download info for book {book_id}:")
        print(f"    other_formats.rsvp              : {rsvp_path!r}")
        print(f"    full download URL               : {rsvp_url!r}")
        print(f"    format_metadata.rsvp.size       : {rsvp_size}")
        print(f"    format_metadata.rsvp.mtime      : {rsvp_mtime!r}")
        print(f"    last_modified (book-level)      : {last_modified!r}")

        # Verify download returns HTTP 200
        print(f"\n[5] Verifying download HEAD/GET {rsvp_url}")
        req = urllib.request.Request(rsvp_url, method="GET")
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                content_length = resp.headers.get("Content-Length", "<missing>")
                content_type = resp.headers.get("Content-Type", "<missing>")
                http_status = resp.status
                body_bytes = len(resp.read())
            print(f"    HTTP status     : {http_status}")
            print(f"    Content-Type    : {content_type!r}")
            print(f"    Content-Length  : {content_length}")
            print(f"    bytes received  : {body_bytes}")
            if http_status == 200:
                print(f"\n    OK — rsvp download confirmed for book {book_id}.")
            else:
                print(f"\n    UNEXPECTED status {http_status} for book {book_id}.", file=sys.stderr)
        except urllib.error.HTTPError as e:
            print(f"    FAILED: HTTP {e.code} {e.reason}", file=sys.stderr)

    print("\nDone.\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Probe calibre-server AJAX endpoints and optionally save JSON fixtures."
    )
    parser.add_argument(
        "--base-url",
        default="http://localhost:8099",
        help="Base URL of calibre-server (default: http://localhost:8099)",
    )
    parser.add_argument(
        "--library",
        default=None,
        help="Library ID to use (default: server's default_library)",
    )
    parser.add_argument(
        "--query",
        default="",
        help="Search query (default: empty = all books)",
    )
    parser.add_argument(
        "--save-fixtures",
        metavar="DIR",
        default=None,
        help="Directory to dump raw JSON fixture files into",
    )
    args = parser.parse_args()

    fixtures_dir = Path(args.save_fixtures) if args.save_fixtures else None

    probe(
        base_url=args.base_url,
        library_id=args.library,
        query=args.query,
        fixtures_dir=fixtures_dir,
    )


if __name__ == "__main__":
    main()
