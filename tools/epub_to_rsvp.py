#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
import pathlib
import posixpath
import re
import textwrap
import urllib.parse
import zipfile
import xml.etree.ElementTree as ET


RSVP_VERSION = "1"
WRAP_WIDTH = 96
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


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def clean_text(text: str) -> str:
    return re.sub(r"\s+", " ", html.unescape(text)).strip()


def directive_text(text: str) -> str:
    return clean_text(text).replace("\n", " ").replace("\r", " ")


def zip_join(base: str, href: str) -> str:
    decoded = urllib.parse.unquote(href.split("#", 1)[0])
    return posixpath.normpath(posixpath.join(posixpath.dirname(base), decoded))


def read_zip_text(epub: zipfile.ZipFile, name: str) -> str:
    return epub.read(name).decode("utf-8-sig", errors="replace")


def first_child_text(root: ET.Element, wanted_name: str) -> str:
    for node in root.iter():
        if local_name(node.tag) == wanted_name and node.text:
            return clean_text(node.text)
    return ""


class XhtmlExtractor(HTMLParser):
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

    manifest: dict[str, str] = {}
    for node in root.iter():
        if local_name(node.tag) == "item":
            item_id = node.attrib.get("id")
            href = node.attrib.get("href")
            if item_id and href:
                manifest[item_id] = zip_join(opf_path, href)

    spine_paths: list[str] = []
    for node in root.iter():
        if local_name(node.tag) != "itemref":
            continue
        idref = node.attrib.get("idref")
        if idref in manifest:
            path = manifest[idref]
            if path.lower().endswith((".xhtml", ".html", ".htm")):
                spine_paths.append(path)

    if not spine_paths:
        raise ValueError("EPUB spine does not contain readable XHTML/HTML documents")

    return title, author, spine_paths


def fallback_chapter_title(path: str, index: int) -> str:
    stem = pathlib.PurePosixPath(path).stem
    cleaned = clean_text(stem.replace("_", " ").replace("-", " "))
    return cleaned or f"Chapter {index}"


def extract_events(epub: zipfile.ZipFile, path: str) -> list[tuple[str, str]]:
    parser = XhtmlExtractor()
    parser.feed(read_zip_text(epub, path))
    parser.close()
    return parser.events


def write_rsvp(epub_path: pathlib.Path, output_path: pathlib.Path) -> None:
    with zipfile.ZipFile(epub_path) as epub:
        opf_path = container_rootfile(epub)
        title, author, spine_paths = parse_package(epub, opf_path)

        lines: list[str] = [
            f"@rsvp {RSVP_VERSION}",
            f"@title {directive_text(title or epub_path.stem)}",
        ]
        author = directive_text(author)
        if author:
            lines.append(f"@author {author}")
        lines.extend(
            [
                f"@source {directive_text(epub_path.name)}",
                "",
            ]
        )

        chapter_count = 0
        for index, spine_path in enumerate(spine_paths, start=1):
            events = extract_events(epub, spine_path)
            if not any(kind == "text" for kind, _ in events):
                continue

            if not any(kind == "chapter" for kind, _ in events):
                events.insert(0, ("chapter", fallback_chapter_title(spine_path, index)))

            for kind, value in events:
                if kind == "chapter":
                    chapter_count += 1
                    lines.append("")
                    lines.append(f"@chapter {directive_text(value)}")
                    continue

                for wrapped in textwrap.wrap(clean_text(value), width=WRAP_WIDTH,
                                             break_long_words=False,
                                             break_on_hyphens=False):
                    if wrapped.startswith("@"):
                        wrapped = "@" + wrapped
                    lines.append(wrapped)

        if chapter_count == 0:
            lines.insert(4, f"@chapter {directive_text(title or epub_path.stem)}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines).strip() + "\n", encoding="utf-8")


def output_path_for(input_path: pathlib.Path, output: pathlib.Path | None) -> pathlib.Path:
    if output is None:
        return input_path.with_suffix(".rsvp")
    if output.exists() and output.is_dir():
        return output / f"{input_path.stem}.rsvp"
    if str(output).endswith(("/", "\\")):
        return output / f"{input_path.stem}.rsvp"
    return output


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert an EPUB into the tiny .rsvp format used by the ESP32 reader."
    )
    parser.add_argument("epub", type=pathlib.Path, help="Input .epub file")
    parser.add_argument(
        "output",
        nargs="?",
        type=pathlib.Path,
        help="Output .rsvp file or destination directory. Defaults beside the EPUB.",
    )
    args = parser.parse_args()

    epub_path = args.epub.expanduser().resolve()
    if not epub_path.is_file():
        raise SystemExit(f"Input EPUB not found: {epub_path}")

    output_path = output_path_for(epub_path, args.output.expanduser() if args.output else None)
    write_rsvp(epub_path, output_path)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
