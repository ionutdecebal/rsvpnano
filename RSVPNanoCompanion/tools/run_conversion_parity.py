#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VECTORS = ROOT / "RSVPNanoCompanion" / "testdata" / "conversion"
LOCAL_GRADLE = ROOT / ".local" / "run_local_gradle.ps1"

TEXT_CASES = [
    ("basic-text-input.txt", "basic-text-expected.rsvp", "Basic Text Vector"),
    ("basic-md-input.md", "basic-md-expected.rsvp", "Basic Markdown Vector"),
    (
        "basic-markdown-input.markdown",
        "basic-markdown-expected.rsvp",
        "Basic Markdown Extension Vector",
    ),
]

HTML_CASES = [
    ("basic-html-input.html", "basic-html-expected.rsvp", "Basic HTML Vector"),
    ("basic-htm-input.htm", "basic-htm-expected.rsvp", "Basic HTM Vector"),
    ("basic-xhtml-input.xhtml", "basic-xhtml-expected.rsvp", "Basic XHTML Vector"),
]

EPUB_CASES = [
    ("sample.epub", "sample-expected.rsvp", "Sample EPUB Vector"),
]


def run(command: list[str], cwd: Path = ROOT) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def assert_same(expected: Path, actual: Path, label: str) -> None:
    expected_text = expected.read_text(encoding="utf-8").replace("\r\n", "\n")
    actual_text = actual.read_text(encoding="utf-8").replace("\r\n", "\n")
    if expected_text != actual_text:
        raise AssertionError(f"{label} output differed from {expected}")


def run_kotlin() -> None:
    if not LOCAL_GRADLE.is_file():
        print(f"Skipping Kotlin parity: {LOCAL_GRADLE} not found")
        return
    run(
        [
            "powershell",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(LOCAL_GRADLE),
            ":conversionCore:testDebugUnitTest",
            ":conversionCore:publishWebConverterJs",
            ":shared:testDebugUnitTest",
            "--no-daemon",
            "--no-configuration-cache",
        ]
    )


def load_python_converter():
    converter_path = ROOT / "tools" / "sd_card_converter" / "convert_books.py"
    spec = importlib.util.spec_from_file_location("rsvp_sd_card_converter", converter_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load {converter_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_python_vector(tmp: Path, input_name: str, expected_name: str, title: str, label: str) -> None:
    module = load_python_converter()
    input_path = VECTORS / input_name
    output_path = tmp / f"python-{input_path.stem}.rsvp"
    _title, author, events = module.events_for_file(input_path)
    writer = module.RsvpWriter(
        title=title,
        author=author,
        source=input_path.name,
        max_words=0,
    )
    for kind, value in events:
        if kind == "chapter":
            writer.add_chapter(value)
        else:
            writer.begin_paragraph()
            writer.add_text(value)
    writer.write_to(output_path, fallback_chapter=title)
    assert_same(VECTORS / expected_name, output_path, label)


def run_python_text(tmp: Path) -> None:
    for input_name, expected_name, title in TEXT_CASES:
        run_python_vector(tmp, input_name, expected_name, title, f"Python {Path(input_name).suffix}")


def run_python_html(tmp: Path) -> None:
    for input_name, expected_name, title in HTML_CASES:
        run_python_vector(tmp, input_name, expected_name, title, f"Python {Path(input_name).suffix}")


def run_python_epub_toc() -> None:
    module = load_python_converter()
    for input_name in ("Dracula-epub.epub", "Dracula-epub3.epub"):
        _title, _author, events = module.events_for_file(VECTORS / input_name)
        chapters = [value for kind, value in events if kind == "chapter"]
        if not any(chapter.startswith("CHAPTER I JONATHAN HARKER") for chapter in chapters):
            raise AssertionError(f"Python EPUB TOC chapter I was not used for {input_name}")
        if not any(chapter.startswith("CHAPTER II JONATHAN HARKER") for chapter in chapters):
            raise AssertionError(f"Python EPUB TOC chapter II was not used for {input_name}")
        if sum(chapter.startswith("CHAPTER I JONATHAN HARKER") for chapter in chapters) != 1:
            raise AssertionError(f"Python EPUB duplicated chapter I for {input_name}")
        if any(chapter == "CHAPTER I" for chapter in chapters):
            raise AssertionError(f"Python EPUB kept short duplicate chapter I for {input_name}")
        if any("7599939443149237915" in chapter for chapter in chapters):
            raise AssertionError(f"Python EPUB used generated XHTML filename chapter for {input_name}")
        if any(chapter == "D R A C U L A" for chapter in chapters):
            raise AssertionError(f"Python EPUB used title-page chapter for {input_name}")

    _title, _author, events = module.events_for_file(VECTORS / "single-document-toc.epub")
    chapters = [value for kind, value in events if kind == "chapter"]
    expected = ["I. The Arrival", "II. Father and Son"]
    if chapters != expected:
        raise AssertionError(f"Python single-document EPUB chapters were {chapters!r}, expected {expected!r}")

    expected_chapters = {
        "nested-toc.epub": ["Part One", "Chapter One A", "Part Two"],
        "encoded-paths.epub": ["Encoded Chapter"],
        "epub3-nav-priority.epub": ["Nav Chapter Title"],
    }
    for input_name, expected in expected_chapters.items():
        _title, _author, events = module.events_for_file(VECTORS / input_name)
        chapters = [value for kind, value in events if kind == "chapter"]
        if chapters != expected:
            raise AssertionError(f"Python {input_name} chapters were {chapters!r}, expected {expected!r}")

    try:
        module.events_for_file(VECTORS / "encrypted-content.epub")
    except Exception:
        pass
    else:
        raise AssertionError("Python encrypted-content.epub should fail conversion")

    tcomc = VECTORS / "TCOMC.epub"
    if tcomc.is_file():
        _title, _author, events = module.events_for_file(tcomc)
        chapters = [value for kind, value in events if kind == "chapter"]
        required = {
            "I MARSEILLE - ARRIVAL",
            "III LES CATALANS",
            "XI THE CORSICAN OGRE",
            "CXVII OCTOBER THE FIFTH",
            "Notes",
        }
        if len(chapters) != 122 or not required.issubset(chapters):
            raise AssertionError(f"Python TCOMC chapters were not complete: {len(chapters)} chapters")
        if "Contents" in chapters or "The Count of Monte Cristo" in chapters:
            raise AssertionError("Python TCOMC kept generated contents/title-page chapters")


def run_web_vector(tmp: Path, command: str, input_name: str, expected_name: str, title: str, label: str) -> None:
    node = shutil.which("node")
    if node is None:
        print("Skipping web parity: node not found")
        return
    output = tmp / f"web-{Path(input_name).stem}.rsvp"
    run(
        [
            node,
            str(ROOT / "web" / "converter_cli.cjs"),
            command,
            str(VECTORS / input_name),
            str(output),
            title,
        ]
    )
    assert_same(VECTORS / expected_name, output, label)


def run_web_text(tmp: Path) -> None:
    for input_name, expected_name, title in TEXT_CASES:
        run_web_vector(tmp, "text", input_name, expected_name, title, f"Web {Path(input_name).suffix}")


def run_web_html(tmp: Path) -> None:
    for input_name, expected_name, title in HTML_CASES:
        run_web_vector(tmp, "html", input_name, expected_name, title, f"Web {Path(input_name).suffix}")


def run_web_epub(tmp: Path) -> None:
    for input_name, expected_name, title in EPUB_CASES:
        run_web_vector(tmp, "book", input_name, expected_name, title, f"Web {Path(input_name).suffix}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run cross-runtime RSVP converter parity checks.")
    parser.add_argument("--skip-kotlin", action="store_true", help="Skip Gradle/Kotlin tests.")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="rsvpnano-conversion-") as temp:
        tmp = Path(temp)
        if not args.skip_kotlin:
            run_kotlin()
        run_python_text(tmp)
        run_python_html(tmp)
        run_python_epub_toc()
        run_web_text(tmp)
        run_web_html(tmp)
        run_web_epub(tmp)

    print("Conversion parity checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
