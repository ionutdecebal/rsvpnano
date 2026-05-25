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
    text = unicodedata.normalize("NFC", text)
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


def normalize_zip_path(path: str) -> str:
    return path.replace("\\", "/").lstrip("/")


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


def iter_output_tokens(text: str):
    for token in re.split(r"\s+", clean_text(text)):
        if token:
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
        readable_words = list(iter_clean_words(text))
        readable_index = 0
        for word in iter_output_tokens(text):
            if self.max_words > 0 and self.word_count >= self.max_words:
                return False

            projected = len(word) if not self._line_words else self._line_length + 1 + len(word)
            if self._line_words and projected > WRAP_WIDTH:
                self.flush_line()

            self._line_words.append(word)
            self._line_length = len(word) if self._line_length == 0 else self._line_length + 1 + len(word)
            if readable_index < len(readable_words) and word == readable_words[readable_index]:
                self.word_count += 1
                readable_index += 1

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

        output_path.write_text("\n".join(self.lines).strip() + "\n", encoding="utf-8")


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


def parse_package(
    epub: zipfile.ZipFile, opf_path: str
) -> tuple[str, str, list[str], dict[str, list[tuple[str, str]]]]:
    package_xml = read_zip_text(epub, opf_path)
    root = ET.fromstring(package_xml)
    title = first_child_text(root, "title")
    author = first_child_text(root, "creator")

    manifest: dict[str, tuple[str, str]] = {}
    nav_paths: list[str] = []
    ncx_paths: list[str] = []
    for node in root.iter():
        if local_name(node.tag) == "item":
            item_id = node.attrib.get("id")
            href = node.attrib.get("href")
            media_type = node.attrib.get("media-type", "")
            if item_id and href:
                resolved_path = zip_join(opf_path, href)
                manifest[item_id] = (resolved_path, media_type)
                if media_type.lower() == "application/x-dtbncx+xml":
                    ncx_paths.append(resolved_path)
                if "nav" in node.attrib.get("properties", "").split():
                    nav_paths.append(resolved_path)

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

    package_version = root.attrib.get("version", "")
    return title, author, spine_paths, toc_titles_by_path(epub, title, package_version, nav_paths, ncx_paths)


def toc_titles_by_path(
    epub: zipfile.ZipFile,
    title: str,
    package_version: str,
    nav_paths: list[str],
    ncx_paths: list[str],
) -> dict[str, list[tuple[str, str]]]:
    primary = (
        [(path, html_nav_toc_titles) for path in nav_paths]
        if package_version.startswith("3")
        else [(path, ncx_toc_titles) for path in ncx_paths]
    )
    fallback = (
        [(path, ncx_toc_titles) for path in ncx_paths]
        if package_version.startswith("3")
        else [(path, html_nav_toc_titles) for path in nav_paths]
    )

    for path, parser in primary:
        try:
            titles = parser(read_zip_text(epub, path), path, title)
        except Exception:
            titles = {}
        if titles:
            return titles

    for path, parser in fallback:
        try:
            titles = parser(read_zip_text(epub, path), path, title)
        except Exception:
            titles = {}
        if titles:
            return titles

    return {}


def ncx_toc_titles(xml: str, toc_path: str, book_title: str) -> dict[str, list[tuple[str, str]]]:
    root = ET.fromstring(xml)
    titles: dict[str, list[tuple[str, str]]] = {}
    for nav_point in root.iter():
        if local_name(nav_point.tag) != "navPoint":
            continue

        label = ""
        src = ""
        for child in nav_point.iter():
            name = local_name(child.tag)
            if name == "text" and not label:
                label = clean_text("".join(child.itertext()))
            elif name == "content" and not src:
                src = child.attrib.get("src", "")

        if src and is_content_toc_title(label, book_title):
            titles.setdefault(toc_path_key(toc_path, src), []).append((label, toc_path_fragment(src)))
    return titles


def html_nav_toc_titles(markup: str, toc_path: str, book_title: str) -> dict[str, list[tuple[str, str]]]:
    parser = HtmlTocExtractor(book_title)
    parser.feed(markup)
    parser.close()
    titles: dict[str, list[tuple[str, str]]] = {}
    for href, title in parser.links:
        titles.setdefault(toc_path_key(toc_path, href), []).append((title, toc_path_fragment(href)))
    return titles


class HtmlTocExtractor(HTMLParser):
    def __init__(self, book_title: str) -> None:
        super().__init__(convert_charrefs=True)
        self.book_title = book_title
        self.links: list[tuple[str, str]] = []
        self._in_toc_nav = False
        self._nav_depth = 0
        self._href: str | None = None
        self._parts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attrs_dict = {key.lower(): value or "" for key, value in attrs}
        tag = tag.lower()
        if tag == "nav" and (
            attrs_dict.get("epub:type") == "toc" or attrs_dict.get("type") == "toc"
        ):
            self._in_toc_nav = True
            self._nav_depth = 1
            return
        if self._in_toc_nav and tag == "nav":
            self._nav_depth += 1
        if self._in_toc_nav and tag == "a":
            self._href = attrs_dict.get("href", "")
            self._parts = []

    def handle_endtag(self, tag: str) -> None:
        tag = tag.lower()
        if self._in_toc_nav and tag == "a" and self._href:
            label = clean_text(" ".join(self._parts))
            if is_content_toc_title(label, self.book_title):
                self.links.append((self._href, label))
            self._href = None
            self._parts = []
            return
        if self._in_toc_nav and tag == "nav":
            self._nav_depth -= 1
            if self._nav_depth <= 0:
                self._in_toc_nav = False

    def handle_data(self, data: str) -> None:
        if self._href is not None:
            self._parts.append(data)


def toc_path_key(toc_path: str, href: str) -> str:
    href = href.split("#", 1)[0].split("?", 1)[0]
    return normalize_zip_path(zip_join(toc_path, href)).lower()


def toc_path_fragment(href: str) -> str:
    if "#" not in href:
        return ""
    return html.unescape(urllib.parse.unquote(href.split("#", 1)[1].split("?", 1)[0])).strip()


def is_content_toc_title(value: str, book_title: str) -> bool:
    cleaned = clean_text(value)
    lowered = cleaned.lower()
    normalized = normalized_toc_label(cleaned)
    normalized_title = normalized_toc_label(book_title)
    return (
        bool(cleaned)
        and lowered not in {"contents", "cover", "title page"}
        and normalized != "tableofcontents"
        and (not normalized_title or normalized != normalized_title)
        and (not normalized_title or not normalized_title.startswith(normalized))
        and any(char.isalnum() for char in cleaned)
    )


def normalized_toc_label(value: str) -> str:
    return "".join(char.lower() for char in clean_text(value) if char.isalnum())


def fallback_chapter_title(path: str, index: int) -> str:
    stem = PurePosixPath(path).stem
    cleaned = clean_text(stem.replace("_", " ").replace("-", " "))
    return cleaned or f"Chapter {index}"


def epub_events_and_metadata(path: Path) -> tuple[str, str, list[tuple[str, str]]]:
    events: list[tuple[str, str]] = []
    with zipfile.ZipFile(path) as epub:
        if any(normalize_zip_path(name).lower() == "meta-inf/encryption.xml" for name in epub.namelist()):
            raise ValueError("This EPUB could not be converted locally")
        opf_path = container_rootfile(epub)
        title, author, spine_paths, toc_titles = parse_package(epub, opf_path)

        for index, spine_path in enumerate(spine_paths, start=1):
            toc_entries = toc_titles.get(normalize_zip_path(spine_path).lower(), [])
            chapter_events = html_events(with_toc_anchor_chapters(read_zip_text(epub, spine_path), toc_entries))
            if not any(kind == "text" for kind, _ in chapter_events):
                continue
            toc_labels = [label for label, _fragment in toc_entries]
            if toc_labels:
                chapter_events = apply_toc_titles(chapter_events, toc_labels, title)
            elif toc_titles:
                if is_generated_table_of_contents(chapter_events) or is_book_title_page(chapter_events, title):
                    continue
            elif not any(kind == "chapter" for kind, _ in chapter_events):
                chapter_events.insert(0, ("chapter", fallback_chapter_title(spine_path, index)))
            events.extend(chapter_events)

    return title or path.stem, author, events


def with_toc_anchor_chapters(markup: str, toc_entries: list[tuple[str, str]]) -> str:
    result = markup
    for label, fragment in toc_entries:
        if not fragment:
            continue
        marker = f"<h1>{html.escape(label)}</h1>"
        attr_pattern = rf"""\s(?:id|name)\s*=\s*(['"]){re.escape(fragment)}\1"""

        heading_match = next(
            (
                match
                for match in re.finditer(
                    r"""<((?:[A-Za-z_][\w.-]*:)?h[1-6])\b([^>]*)>.*?</\1>""",
                    result,
                    flags=re.IGNORECASE | re.DOTALL,
                )
                if re.search(attr_pattern, match.group(2), flags=re.IGNORECASE)
            ),
            None,
        )
        if heading_match:
            result = result[: heading_match.start()] + marker + result[heading_match.end() :]
            continue

        tag_match = next(
            (
                match
                for match in re.finditer(r"""<([A-Za-z_][\w.:-]*)([^>]*)>""", result, flags=re.IGNORECASE)
                if re.search(attr_pattern, match.group(2), flags=re.IGNORECASE)
            ),
            None,
        )
        if tag_match:
            result = result[: tag_match.start()] + marker + tag_match.group(0) + result[tag_match.end() :]
    return result


def apply_toc_titles(
    events: list[tuple[str, str]], toc_titles: list[str], book_title: str
) -> list[tuple[str, str]]:
    result = remove_chapter_matching(events, book_title)

    if len(toc_titles) == 1:
        result = remove_chapter_matching(result, toc_titles[0])
        result = remove_first_chapter_prefix_of(result, toc_titles[0])
        result = remove_first_chapter(result)
        result.insert(0, ("chapter", toc_titles[0]))
        result = remove_chapters_after_first(result)
        return result

    output: list[tuple[str, str]] = []
    toc_index = 0
    for kind, value in result:
        if kind == "chapter":
            if toc_index < len(toc_titles):
                output.append(("chapter", toc_titles[toc_index]))
                toc_index += 1
            continue
        output.append((kind, value))

    while toc_index < len(toc_titles):
        output.append(("chapter", toc_titles[toc_index]))
        toc_index += 1
    return output


def remove_first_chapter(events: list[tuple[str, str]]) -> list[tuple[str, str]]:
    result = list(events)
    for index, (kind, _value) in enumerate(result):
        if kind == "chapter":
            del result[index]
            break
    return result


def remove_first_chapter_matching(events: list[tuple[str, str]], title: str) -> list[tuple[str, str]]:
    normalized = clean_text(title).lower()
    result = list(events)
    for index, (kind, value) in enumerate(result):
        if kind == "chapter" and clean_text(value).lower() == normalized:
            del result[index]
            break
    return result


def remove_first_chapter_prefix_of(events: list[tuple[str, str]], title: str) -> list[tuple[str, str]]:
    normalized = clean_text(title).lower()
    result = list(events)
    for index, (kind, value) in enumerate(result):
        chapter = clean_text(value).lower()
        if kind == "chapter" and normalized.startswith(f"{chapter} "):
            del result[index]
            break
    return result


def remove_chapter_matching(events: list[tuple[str, str]], title: str) -> list[tuple[str, str]]:
    normalized = clean_text(title).lower()
    normalized_toc = normalized_toc_label(title)
    if not normalized:
        return events
    return [
        (kind, value)
        for kind, value in events
        if not (
            kind == "chapter"
            and (clean_text(value).lower() == normalized or normalized_toc_label(value) == normalized_toc)
        )
    ]


def remove_chapters_after_first(events: list[tuple[str, str]]) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    seen_first = False
    for kind, value in events:
        if kind != "chapter":
            result.append((kind, value))
        elif not seen_first:
            result.append((kind, value))
            seen_first = True
    return result


def is_generated_table_of_contents(events: list[tuple[str, str]]) -> bool:
    first_chapter = next((value for kind, value in events if kind == "chapter"), "")
    normalized = clean_text(first_chapter).lower()
    if normalized not in {"contents", "table of contents"}:
        return False
    link_like_count = 0
    text_count = 0
    for kind, value in events:
        if kind != "text":
            continue
        text_count += 1
        cleaned = clean_text(value).lower()
        if re.fullmatch(r"[ivxlcdm]+|[ivxlcdm]+\s+.+", cleaned):
            link_like_count += 1
        if link_like_count >= 20:
            return True
    return text_count >= 4


def is_book_title_page(events: list[tuple[str, str]], book_title: str) -> bool:
    normalized_title = normalized_toc_label(book_title)
    normalized_chapter = next(
        (
            normalized_toc_label(value)
            for kind, value in events
            if kind == "chapter"
            and normalized_toc_label(value)
            and normalized_title.startswith(normalized_toc_label(value))
        ),
        "",
    )
    text_count = sum(1 for kind, _value in events if kind == "text")
    return (
        bool(normalized_title)
        and bool(normalized_chapter)
        and normalized_title.startswith(normalized_chapter)
        and (normalized_title == normalized_chapter or len(normalized_chapter) >= 8)
        and text_count <= 25
    )


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
