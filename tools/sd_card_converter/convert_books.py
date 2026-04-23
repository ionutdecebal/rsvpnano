#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
from pathlib import Path, PurePosixPath
import posixpath
import re
import sys
import unicodedata
import urllib.parse
import zipfile
import xml.etree.ElementTree as ET


RSVP_VERSION = "1"
WRAP_WIDTH = 96
DEFAULT_MAX_WORDS = 0
BOOKS_DIR_NAME = "books"

SUPPORTED_EXTENSIONS = {
    ".epub",
    ".html",
    ".htm",
    ".xhtml",
    ".md",
    ".markdown",
    ".txt",
}

SIDE_CAR_SUFFIXES = (
    ".rsvp.failed",
    ".rsvp.tmp",
    ".rsvp.converting",
)

BLOCK_TAGS = {
    "address",
    "article",
    "aside",
    "blockquote",
    "body",
    "br",
    "dd",
    "div",
    "dl",
    "dt",
    "figcaption",
    "figure",
    "footer",
    "header",
    "hr",
    "li",
    "main",
    "ol",
    "p",
    "pre",
    "section",
    "table",
    "tbody",
    "td",
    "tfoot",
    "th",
    "thead",
    "tr",
    "ul",
}

HEADING_TAGS = {"h1", "h2", "h3", "h4", "h5", "h6"}
SKIP_TAGS = {"head", "math", "nav", "script", "style", "svg"}
TRIMMABLE_EDGE_CHARS = "\"'()[]{}<>"

UNICODE_ASCII_REPLACEMENTS = str.maketrans(
    {
        "\u00a0": " ",
        "\u1680": " ",
        "\u180e": " ",
        "\u2000": " ",
        "\u2001": " ",
        "\u2002": " ",
        "\u2003": " ",
        "\u2004": " ",
        "\u2005": " ",
        "\u2006": " ",
        "\u2007": " ",
        "\u2008": " ",
        "\u2009": " ",
        "\u200a": " ",
        "\u2028": " ",
        "\u2029": " ",
        "\u202f": " ",
        "\u205f": " ",
        "\u3000": " ",
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u2035": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u201f": '"',
        "\u00ab": '"',
        "\u00bb": '"',
        "\u2033": '"',
        "\u2036": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2015": "-",
        "\u2043": "-",
        "\u2212": "-",
        "\u2026": "...",
        "\u2022": "*",
        "\u00b7": "*",
        "\u2219": "*",
        "\u00a9": "(c)",
        "\u00ae": "(r)",
        "\u2122": "TM",
        "\ufb00": "ff",
        "\ufb01": "fi",
        "\ufb02": "fl",
        "\ufb03": "ffi",
        "\ufb04": "ffl",
        "\ufb05": "st",
        "\ufb06": "st",
        "\ufffd": "",
    }
)


def clean_text(text: str) -> str:
    text = html.unescape(text).translate(UNICODE_ASCII_REPLACEMENTS)
    text = unicodedata.normalize("NFKD", text)
    text = text.encode("ascii", errors="ignore").decode("ascii")
    return re.sub(r"\s+", " ", text).strip()


def directive_text(text: str) -> str:
    return clean_text(text).replace("\n", " ").replace("\r", " ")


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def zip_join(base: str, href: str) -> str:
    decoded = urllib.parse.unquote(href.split("#", 1)[0].split("?", 1)[0])
    if decoded.startswith("/"):
        decoded = decoded.lstrip("/")
    return posixpath.normpath(posixpath.join(posixpath.dirname(base), decoded))


def read_text_file(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def read_zip_text(epub: zipfile.ZipFile, name: str) -> str:
    return epub.read(name).decode("utf-8-sig", errors="replace")


def first_child_text(root: ET.Element, wanted_name: str) -> str:
    for node in root.iter():
        if local_name(node.tag) == wanted_name and node.text:
            return clean_text(node.text)
    return ""


def looks_like_chapter(line: str) -> str | None:
    trimmed = clean_text(line)
    if not trimmed or len(trimmed) > 64:
        return None

    if trimmed.startswith("#"):
        title = trimmed.lstrip("#").strip()
        return title or None

    if re.match(r"^(chapter|part|book)\b", trimmed, flags=re.IGNORECASE):
        return trimmed

    return None


def iter_clean_words(text: str):
    for raw in re.split(r"\s+", clean_text(text)):
        token = raw.strip(TRIMMABLE_EDGE_CHARS)
        if token and any(ch.isalnum() for ch in token):
            yield token


class RsvpWriter:
    def __init__(self, title: str, source: str, max_words: int, author: str = "") -> None:
        self.lines: list[str] = [
            f"@rsvp {RSVP_VERSION}",
            f"@title {directive_text(title)}",
        ]
        author = directive_text(author)
        if author:
            self.lines.append(f"@author {author}")
        self.lines.extend(
            [
                f"@source {directive_text(source)}",
                "",
            ]
        )
        self.max_words = max_words
        self.word_count = 0
        self.chapter_count = 0
        self._line_words: list[str] = []
        self._line_length = 0
        self._last_chapter = ""

    def add_chapter(self, title: str) -> None:
        title = directive_text(title)
        if not title or title == self._last_chapter:
            return
        self.flush_line()
        if self.lines and self.lines[-1] != "":
            self.lines.append("")
        self.lines.append(f"@chapter {title}")
        self.chapter_count += 1
        self._last_chapter = title

    def begin_paragraph(self) -> None:
        self.flush_line()
        if self.word_count > 0:
            if self.lines and self.lines[-1] != "":
                self.lines.append("")
            self.lines.append("@para")

    def add_text(self, text: str) -> bool:
        for word in iter_clean_words(text):
            if self.max_words > 0 and self.word_count >= self.max_words:
                return False

            projected = len(word) if not self._line_words else self._line_length + 1 + len(word)
            if self._line_words and projected > WRAP_WIDTH:
                self.flush_line()

            self._line_words.append(word)
            self._line_length = len(word) if self._line_length == 0 else self._line_length + 1 + len(word)
            self.word_count += 1

        return True

    def flush_line(self) -> None:
        if not self._line_words:
            return
        line = " ".join(self._line_words)
        if line.startswith("@"):
            line = "@" + line
        self.lines.append(line)
        self._line_words = []
        self._line_length = 0

    def write_to(self, output_path: Path, fallback_chapter: str) -> None:
        self.flush_line()
        if self.word_count == 0:
            raise ValueError("no readable words found")
        if self.chapter_count == 0:
            self.lines.insert(4, f"@chapter {directive_text(fallback_chapter)}")

        output_path.write_text("\n".join(self.lines).strip() + "\n", encoding="ascii")


class HtmlEventsExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.events: list[tuple[str, str]] = []
        self._skip_depth = 0
        self._heading_tag: str | None = None
        self._heading_parts: list[str] = []
        self._text_parts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        del attrs
        tag = tag.lower()
        if tag in SKIP_TAGS:
            self._skip_depth += 1
            return
        if self._skip_depth > 0:
            return
        if tag in HEADING_TAGS:
            self._flush_text()
            self._heading_tag = tag
            self._heading_parts = []
            return
        if tag == "br":
            self._flush_text()

    def handle_endtag(self, tag: str) -> None:
        tag = tag.lower()
        if tag in SKIP_TAGS and self._skip_depth > 0:
            self._skip_depth -= 1
            return
        if self._skip_depth > 0:
            return
        if self._heading_tag == tag:
            title = clean_text(" ".join(self._heading_parts))
            if title:
                self.events.append(("chapter", title))
            self._heading_tag = None
            self._heading_parts = []
            return
        if tag in BLOCK_TAGS:
            self._flush_text()

    def handle_data(self, data: str) -> None:
        if self._skip_depth > 0:
            return
        if self._heading_tag is not None:
            self._heading_parts.append(data)
            return
        self._text_parts.append(data)

    def close(self) -> None:
        super().close()
        self._flush_text()

    def _flush_text(self) -> None:
        text = clean_text(" ".join(self._text_parts))
        self._text_parts = []
        if text:
            self.events.append(("text", text))


def html_events(markup: str) -> list[tuple[str, str]]:
    parser = HtmlEventsExtractor()
    parser.feed(markup)
    parser.close()
    return parser.events


def text_events(text: str) -> list[tuple[str, str]]:
    events: list[tuple[str, str]] = []
    paragraph_parts: list[str] = []

    def flush_paragraph() -> None:
        if paragraph_parts:
            events.append(("text", clean_text(" ".join(paragraph_parts))))
            paragraph_parts.clear()

    for raw_line in text.splitlines():
        line = raw_line.strip()
        chapter = looks_like_chapter(line)
        if chapter:
            flush_paragraph()
            events.append(("chapter", chapter))
            continue

        if not line:
            flush_paragraph()
            continue

        paragraph_parts.append(line)

    flush_paragraph()
    return events


def container_rootfile(epub: zipfile.ZipFile) -> str:
    container_xml = read_zip_text(epub, "META-INF/container.xml")
    root = ET.fromstring(container_xml)
    for node in root.iter():
        if local_name(node.tag) == "rootfile":
            full_path = node.attrib.get("full-path", "")
            if full_path:
                return full_path
    raise ValueError("EPUB container.xml does not name an OPF package file")


def parse_package(epub: zipfile.ZipFile, opf_path: str) -> tuple[str, str, list[str]]:
    package_xml = read_zip_text(epub, opf_path)
    root = ET.fromstring(package_xml)
    title = first_child_text(root, "title")
    author = first_child_text(root, "creator")

    manifest: dict[str, tuple[str, str]] = {}
    for node in root.iter():
        if local_name(node.tag) == "item":
            item_id = node.attrib.get("id")
            href = node.attrib.get("href")
            media_type = node.attrib.get("media-type", "")
            if item_id and href:
                manifest[item_id] = (zip_join(opf_path, href), media_type)

    spine_paths: list[str] = []
    for node in root.iter():
        if local_name(node.tag) != "itemref":
            continue
        idref = node.attrib.get("idref")
        if idref not in manifest:
            continue

        path, media_type = manifest[idref]
        lowered = path.lower()
        if media_type in {"application/xhtml+xml", "text/html"} or lowered.endswith(
            (".xhtml", ".html", ".htm")
        ):
            spine_paths.append(path)

    if not spine_paths:
        raise ValueError("EPUB spine does not contain readable XHTML/HTML documents")

    return title, author, spine_paths


def fallback_chapter_title(path: str, index: int) -> str:
    stem = PurePosixPath(path).stem
    cleaned = clean_text(stem.replace("_", " ").replace("-", " "))
    return cleaned or f"Chapter {index}"


def epub_events_and_metadata(path: Path) -> tuple[str, str, list[tuple[str, str]]]:
    events: list[tuple[str, str]] = []
    with zipfile.ZipFile(path) as epub:
        opf_path = container_rootfile(epub)
        title, author, spine_paths = parse_package(epub, opf_path)

        for index, spine_path in enumerate(spine_paths, start=1):
            chapter_events = html_events(read_zip_text(epub, spine_path))
            if not any(kind == "text" for kind, _ in chapter_events):
                continue
            if not any(kind == "chapter" for kind, _ in chapter_events):
                chapter_events.insert(0, ("chapter", fallback_chapter_title(spine_path, index)))
            events.extend(chapter_events)

    return title or path.stem, author, events


def events_for_file(path: Path) -> tuple[str, str, list[tuple[str, str]]]:
    suffix = path.suffix.lower()
    if suffix == ".epub":
        return epub_events_and_metadata(path)
    if suffix in {".html", ".htm", ".xhtml"}:
        return path.stem, "", html_events(read_text_file(path))
    if suffix in {".txt", ".md", ".markdown"}:
        return path.stem, "", text_events(read_text_file(path))
    raise ValueError(f"unsupported extension: {path.suffix}")


def output_path_for(path: Path) -> Path:
    return path.with_suffix(".rsvp")


def cleanup_sidecars(output_path: Path) -> None:
    stem = output_path.with_suffix("")
    for suffix in SIDE_CAR_SUFFIXES:
        sidecar = stem.with_name(stem.name + suffix)
        if sidecar.exists() and sidecar.is_file():
            sidecar.unlink()


def convert_one(path: Path, force: bool, max_words: int) -> tuple[str, str]:
    output_path = output_path_for(path)
    if output_path.exists() and not force:
        return "skipped", f"{path.name} already has {output_path.name}"

    title, author, events = events_for_file(path)
    writer = RsvpWriter(title=title, author=author, source=path.name, max_words=max_words)
    for kind, value in events:
        if kind == "chapter":
            writer.add_chapter(value)
            continue
        writer.begin_paragraph()
        if not writer.add_text(value):
            break

    writer.write_to(output_path, fallback_chapter=title or path.stem)
    cleanup_sidecars(output_path)
    return "converted", f"{path.name} -> {output_path.name} ({writer.word_count} words)"


def default_root() -> Path:
    script_dir = Path(__file__).resolve().parent
    if script_dir.name.lower() == BOOKS_DIR_NAME:
        return script_dir.parent
    if (script_dir / BOOKS_DIR_NAME).is_dir():
        return script_dir
    if (script_dir.parent / BOOKS_DIR_NAME).is_dir():
        return script_dir.parent
    return script_dir


def books_dir_for(root: Path) -> Path:
    root = root.expanduser().resolve()
    if root.name.lower() == BOOKS_DIR_NAME:
        return root
    return root / BOOKS_DIR_NAME


def candidate_books(books_dir: Path) -> list[Path]:
    candidates: list[Path] = []
    for path in sorted(books_dir.iterdir(), key=lambda item: item.name.lower()):
        if not path.is_file() or path.name.startswith("."):
            continue
        if path.suffix.lower() == ".rsvp":
            continue
        if path.name.lower().endswith(SIDE_CAR_SUFFIXES):
            continue
        if path.suffix.lower() in SUPPORTED_EXTENSIONS:
            candidates.append(path)
    return candidates


def run(root: Path, force: bool, max_words: int) -> int:
    books_dir = books_dir_for(root)
    if not books_dir.is_dir():
        print(f"Could not find a '{BOOKS_DIR_NAME}' folder at: {books_dir}")
        print("Place this script at the SD card root, next to the books folder.")
        return 2

    candidates = candidate_books(books_dir)
    if not candidates:
        print(f"No supported non-RSVP books found in {books_dir}")
        return 0

    converted = 0
    skipped = 0
    failed = 0
    print(f"Scanning {books_dir}")
    print(f"Max words per book: {'unlimited' if max_words <= 0 else max_words}")
    print()

    for path in candidates:
        try:
            status, message = convert_one(path, force=force, max_words=max_words)
        except Exception as exc:
            failed += 1
            print(f"[failed] {path.name}: {exc}")
            continue

        if status == "converted":
            converted += 1
        else:
            skipped += 1
        print(f"[{status}] {message}")

    print()
    print(f"Converted: {converted}")
    print(f"Skipped:   {skipped}")
    print(f"Failed:    {failed}")

    if failed:
        return 1
    return 0


def pause_if_double_clicked() -> None:
    if sys.stdin.isatty():
        try:
            input("\nPress Enter to close...")
        except EOFError:
            pass


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Convert supported books in an RSVP Nano SD card /books folder into .rsvp files."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=default_root(),
        help="SD card root. Defaults to the folder containing this script.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Regenerate .rsvp files even when they already exist.",
    )
    parser.add_argument(
        "--max-words",
        type=int,
        default=DEFAULT_MAX_WORDS,
        help="Maximum words per generated book. Defaults to 0 (unlimited).",
    )
    parser.add_argument(
        "--no-pause",
        action="store_true",
        help="Do not wait for Enter before closing.",
    )
    args = parser.parse_args()

    try:
        return run(root=args.root, force=args.force, max_words=args.max_words)
    finally:
        if not args.no_pause:
            pause_if_double_clicked()


if __name__ == "__main__":
    raise SystemExit(main())
