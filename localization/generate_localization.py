#!/usr/bin/env python3
"""Generate RSVP Nano localization C++ from localization/strings.toml.

The firmware output stays intentionally tiny:
- UiLanguage and UiText enums generated from TOML order
- one deduplicated NUL-terminated UTF-8 string blob
- a std::array<uint16_t> offset table indexed as [language][text]
- default-language fallback for missing translations

Requires Python 3.11+ for the standard-library tomllib module.
"""

from __future__ import annotations

import argparse
import difflib
import re
import sys
import tomllib
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TypeAlias

TomlTable: TypeAlias = Mapping[str, Any]

IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
UINT16_MAX = 0xFFFF
MISSING_OFFSET = UINT16_MAX
MAX_UINT8_ENUM_ITEMS = 255

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_TOML = REPO_ROOT / "localization" / "strings.toml"
DEFAULT_HEADER = REPO_ROOT / "src" / "ui" / "Localization.h"
DEFAULT_CPP = REPO_ROOT / "src" / "ui" / "Localization.generated.cpp"


class LocalizationError(ValueError):
	"""Raised when the localization source is invalid."""


@dataclass(frozen=True, slots=True)
class Language:
	name: str
	code: str
	label: str


@dataclass(frozen=True, slots=True)
class LocalizedText:
	key: str
	context: str
	max_len: int | None
	values: dict[str, str]


@dataclass(frozen=True, slots=True)
class LocalizationModel:
	default_language: str
	ascii_only: bool
	languages: list[Language]
	texts: list[LocalizedText]

	@property
	def default_language_index(self) -> int:
		for index, language in enumerate(self.languages):
			if language.name == self.default_language:
				return index
		raise LocalizationError(f"default language {self.default_language!r} is not loaded")


@dataclass(frozen=True, slots=True)
class GeneratedFiles:
	header: str
	cpp: str


class StringBlobBuilder:
	"""Builds a deduplicated, NUL-terminated byte blob addressable by uint16_t."""

	def __init__(self) -> None:
		self._blob = bytearray()
		self._offset_by_value: dict[str, int] = {}

	@property
	def size(self) -> int:
		return len(self._blob)

	def add(self, value: str) -> int:
		if value in self._offset_by_value:
			return self._offset_by_value[value]

		offset = len(self._blob)
		encoded = value.encode("utf-8") + b"\0"
		end_offset = offset + len(encoded)

		if offset >= MISSING_OFFSET or end_offset > MISSING_OFFSET:
			raise LocalizationError(
				"localization string blob exceeded uint16_t offset range"
			)

		self._blob.extend(encoded)
		self._offset_by_value[value] = offset
		return offset

	def entries_by_offset(self) -> list[tuple[int, str]]:
		return sorted((offset, value) for value, offset in self._offset_by_value.items())


@dataclass(frozen=True, slots=True)
class OffsetData:
	blob: StringBlobBuilder
	language_name_offsets: list[int]
	text_offsets: list[list[int]]


class CodeWriter:
	"""Small helper for deterministic generated code."""

	def __init__(self) -> None:
		self._lines: list[str] = []

	def add(self, line: str = "") -> None:
		self._lines.append(line)

	def extend(self, lines: Sequence[str]) -> None:
		self._lines.extend(lines)

	def render(self) -> str:
		return "\n".join(self._lines) + "\n"


def read_toml(path: Path) -> TomlTable:
	with path.open("rb") as file:
		return tomllib.load(file)


def as_table(value: Any, field: str) -> TomlTable:
	if not isinstance(value, Mapping):
		raise LocalizationError(f"{field} must be a TOML table")
	return value


def as_table_list(value: Any, field: str) -> list[TomlTable]:
	if not isinstance(value, list) or not value:
		raise LocalizationError(f"{field} must be a non-empty array of tables")

	tables: list[TomlTable] = []
	for index, item in enumerate(value):
		tables.append(as_table(item, f"{field}[{index}]"))
	return tables


def as_string(value: Any, field: str, *, allow_empty: bool = False) -> str:
	if not isinstance(value, str):
		raise LocalizationError(f"{field} must be a string")
	if not allow_empty and value == "":
		raise LocalizationError(f"{field} must be a non-empty string")
	return value


def as_optional_string(value: Any, field: str) -> str:
	return "" if value is None else as_string(value, field, allow_empty=True)


def as_bool(value: Any, field: str) -> bool:
	if not isinstance(value, bool):
		raise LocalizationError(f"{field} must be true or false")
	return value


def as_positive_int_or_none(value: Any, field: str) -> int | None:
	if value is None:
		return None
	if not isinstance(value, int) or value <= 0:
		raise LocalizationError(f"{field} must be a positive integer")
	return value


def require_identifier(value: str, field: str) -> None:
	if not IDENTIFIER_RE.fullmatch(value):
		raise LocalizationError(f"{field} must be a valid C++ identifier, got {value!r}")


def require_ascii(value: str, field: str) -> None:
	try:
		value.encode("ascii")
	except UnicodeEncodeError as exc:
		raise LocalizationError(
			f"{field} contains non-ASCII text while ascii_only=true: {value!r}"
		) from exc


def validate_enum_count(count: int, enum_name: str) -> None:
	if count > MAX_UINT8_ENUM_ITEMS:
		raise LocalizationError(
			f"{enum_name} uses uint8_t; at most {MAX_UINT8_ENUM_ITEMS} entries are supported"
		)


def load_languages(data: TomlTable) -> list[Language]:
	raw_order = data.get("language_order")
	if not isinstance(raw_order, list) or not raw_order:
		raise LocalizationError("language_order must be a non-empty array")

	language_tables = as_table(data.get("languages"), "languages")

	languages: list[Language] = []
	seen: set[str] = set()

	for order_index, raw_name in enumerate(raw_order):
		name = as_string(raw_name, f"language_order[{order_index}]")
		require_identifier(name, f"language_order[{order_index}]")

		if name in seen:
			raise LocalizationError(f"duplicate language {name!r}")
		seen.add(name)

		table = as_table(language_tables.get(name), f"languages.{name}")
		languages.append(
			Language(
				name=name,
				code=as_string(table.get("code"), f"languages.{name}.code"),
				label=as_string(table.get("name"), f"languages.{name}.name"),
			)
		)

	validate_enum_count(len(languages), "UiLanguage")
	return languages


def load_texts(
	data: TomlTable,
	languages: Sequence[Language],
	default_language: str,
	ascii_only: bool,
) -> list[LocalizedText]:
	texts: list[LocalizedText] = []
	seen_keys: set[str] = set()

	for text_index, raw in enumerate(as_table_list(data.get("strings"), "strings")):
		key = as_string(raw.get("key"), f"strings[{text_index}].key")
		require_identifier(key, f"strings[{text_index}].key")

		if key in seen_keys:
			raise LocalizationError(f"duplicate string key {key!r}")
		seen_keys.add(key)

		max_len = as_positive_int_or_none(raw.get("max_len"), f"strings.{key}.max_len")
		values = read_translations(raw, key, languages, ascii_only, max_len)

		if not values.get(default_language):
			raise LocalizationError(
				f"{key} is missing required default-language translation {default_language}"
			)

		texts.append(
			LocalizedText(
				key=key,
				context=as_optional_string(raw.get("context"), f"strings.{key}.context"),
				max_len=max_len,
				values=values,
			)
		)

	validate_enum_count(len(texts), "UiText")
	return texts


def read_translations(
	table: TomlTable,
	key: str,
	languages: Sequence[Language],
	ascii_only: bool,
	max_len: int | None,
) -> dict[str, str]:
	values: dict[str, str] = {}

	for language in languages:
		field = f"strings.{key}.{language.name}"
		raw_value = table.get(language.name)

		if raw_value is None:
			values[language.name] = ""
			continue

		value = as_string(raw_value, field, allow_empty=True)
		if "\0" in value:
			raise LocalizationError(f"{field} contains a NUL byte")
		if ascii_only:
			require_ascii(value, field)
		if max_len is not None and len(value) > max_len:
			raise LocalizationError(
				f"{field} is {len(value)} characters, exceeding max_len={max_len}"
			)

		values[language.name] = value

	return values


def load_model(path: Path) -> LocalizationModel:
	data = read_toml(path)

	if data.get("schema_version") != 1:
		raise LocalizationError("schema_version must be 1")

	default_language = as_string(data.get("default_language", "English"), "default_language")
	ascii_only = as_bool(data.get("ascii_only", False), "ascii_only")
	languages = load_languages(data)

	if default_language not in {language.name for language in languages}:
		raise LocalizationError(f"default_language {default_language!r} is not in language_order")

	return LocalizationModel(
		default_language=default_language,
		ascii_only=ascii_only,
		languages=languages,
		texts=load_texts(data, languages, default_language, ascii_only),
	)


def cxx_string_literal(value: str) -> str:
	def escape_char(char: str) -> str:
		code = ord(char)
		match char:
			case "\\":
				return "\\\\"
			case '"':
				return '\\"'
			case "\0":
				return "\\0"
			case "\n":
				return "\\n"
			case "\r":
				return "\\r"
			case "\t":
				return "\\t"
			case "\b":
				return "\\b"
			case "\f":
				return "\\f"
			case _ if 0x20 <= code <= 0x7E:
				return char
			case _ if code <= 0xFFFF:
				return f"\\u{code:04X}"
			case _:
				return f"\\U{code:08X}"

	return '"' + "".join(escape_char(char) for char in value) + '"'


def build_offsets(model: LocalizationModel) -> OffsetData:
	blob = StringBlobBuilder()
	language_name_offsets = [blob.add(language.label) for language in model.languages]
	text_offsets = [
		[
			MISSING_OFFSET if (value := text.values.get(language.name, "")) == "" else blob.add(value)
			for text in model.texts
		]
		for language in model.languages
	]

	return OffsetData(
		blob=blob,
		language_name_offsets=language_name_offsets,
		text_offsets=text_offsets,
	)


def generated_banner() -> list[str]:
	return [
		"// Generated by tools/generate_localization.py. Do not edit by hand.",
		"// Edit localization/strings.toml, then run the generator.",
	]


def generate_header(model: LocalizationModel) -> str:
	writer = CodeWriter()
	writer.extend(generated_banner())
	writer.add("#pragma once")
	writer.add()
	writer.add("#include <stdint.h>")
	writer.add()
	write_enum(writer, "UiLanguage", [language.name for language in model.languages])
	writer.add()
	write_enum(writer, "UiText", [text.key for text in model.texts])
	writer.add()
	writer.add("namespace Localization {")
	writer.add()
	writer.add("UiLanguage sanitizeLanguage(uint8_t value);")
	writer.add("UiLanguage nextLanguage(UiLanguage current);")
	writer.add("const char *languageName(UiLanguage language);")
	writer.add("const char *text(UiLanguage language, UiText key);")
	writer.add()
	writer.add("}  // namespace Localization")
	return writer.render()


def write_enum(writer: CodeWriter, enum_name: str, values: Sequence[str]) -> None:
	writer.add(f"enum class {enum_name} : uint8_t {{")
	for index, value in enumerate(values):
		initializer = " = 0" if index == 0 else ""
		writer.add(f"\t{value}{initializer},")
	writer.add("\tCount,")
	writer.add("};")


def generate_cpp(model: LocalizationModel) -> str:
	offsets = build_offsets(model)
	writer = CodeWriter()

	writer.extend(generated_banner())
	writer.add('#include "Localization.h"')
	writer.add()
	writer.add("#include <array>")
	writer.add("#include <cstddef>")
	writer.add()
	writer.add("namespace {")
	writer.add()
	writer.add("constexpr size_t kLanguageCount = static_cast<size_t>(UiLanguage::Count);")
	writer.add("constexpr size_t kTextCount = static_cast<size_t>(UiText::Count);")
	writer.add(f"constexpr size_t kDefaultLanguageIndex = {model.default_language_index};")
	writer.add("constexpr uint16_t kMissingOffset = 0xFFFF;")
	writer.add()
	writer.add(
		f'static_assert(kLanguageCount == {len(model.languages)}, "UiLanguage count mismatch");'
	)
	writer.add(f'static_assert(kTextCount == {len(model.texts)}, "UiText count mismatch");')
	writer.add()
	writer.extend(generate_string_blob(offsets.blob))
	writer.add()
	writer.extend(generate_language_name_offsets(model, offsets.language_name_offsets))
	writer.add()
	writer.extend(generate_text_offsets(model, offsets.text_offsets))
	writer.add()
	writer.add("size_t languageIndex(UiLanguage language) {")
	writer.add("\tconst size_t value = static_cast<size_t>(language);")
	writer.add("\treturn value < kLanguageCount ? value : kDefaultLanguageIndex;")
	writer.add("}")
	writer.add()
	writer.add("}  // namespace")
	writer.add()
	writer.add("namespace Localization {")
	writer.add()
	writer.extend(generate_localization_functions())
	writer.add()
	writer.add("}  // namespace Localization")

	return writer.render()


def generate_string_blob(blob: StringBlobBuilder) -> list[str]:
	lines = ["constexpr char kStringBlob[] ="]

	for offset, value in blob.entries_by_offset():
		lines.append(f"\t/* {offset:5d} */ {cxx_string_literal(value + chr(0))}")

	lines[-1] += ";"
	return lines


def generate_language_name_offsets(
	model: LocalizationModel,
	language_name_offsets: Sequence[int],
) -> list[str]:
	lines = ["constexpr std::array<uint16_t, kLanguageCount> kLanguageNameOffsets = {{"]
	lines.extend(
		f"\t/* {language.name:<8} */ {offset},"
		for language, offset in zip(model.languages, language_name_offsets, strict=True)
	)
	lines.append("}};")
	return lines


def generate_text_offsets(model: LocalizationModel, text_offsets: Sequence[Sequence[int]]) -> list[str]:
	lines = [
		"using TextOffsetRow = std::array<uint16_t, kTextCount>;",
		"using TextOffsetTable = std::array<TextOffsetRow, kLanguageCount>;",
		"",
		"constexpr TextOffsetTable kTextOffsets = {{",
	]

	for language, row in zip(model.languages, text_offsets, strict=True):
		lines.append(f"\t// {language.name} ({language.code})")
		lines.append("\t{{")
		lines.extend(
			format_offset_entry(text.key, offset)
			for text, offset in zip(model.texts, row, strict=True)
		)
		lines.append("\t}},")

	lines.append("}};")
	return lines


def format_offset_entry(key: str, offset: int) -> str:
	value = "kMissingOffset" if offset == MISSING_OFFSET else str(offset)
	return f"\t\t/* {key:<24} */ {value},"


def generate_localization_functions() -> list[str]:
	return [
		"UiLanguage sanitizeLanguage(uint8_t value) {",
		"\tif (value >= kLanguageCount) {",
		"\t\treturn static_cast<UiLanguage>(kDefaultLanguageIndex);",
		"\t}",
		"\treturn static_cast<UiLanguage>(value);",
		"}",
		"",
		"UiLanguage nextLanguage(UiLanguage current) {",
		"\tconst size_t value = languageIndex(current);",
		"\treturn static_cast<UiLanguage>((value + 1) % kLanguageCount);",
		"}",
		"",
		"const char *languageName(UiLanguage language) {",
		"\treturn &kStringBlob[kLanguageNameOffsets[languageIndex(language)]];",
		"}",
		"",
		"const char *text(UiLanguage language, UiText key) {",
		"\tconst size_t textIndex = static_cast<size_t>(key);",
		"\tif (textIndex >= kTextCount) {",
		"\t\treturn \"\";",
		"\t}",
		"",
		"\tconst size_t lang = languageIndex(language);",
		"\tuint16_t offset = kTextOffsets[lang][textIndex];",
		"",
		"\tif (offset == kMissingOffset) {",
		"\t\toffset = kTextOffsets[kDefaultLanguageIndex][textIndex];",
		"\t}",
		"",
		"\treturn offset == kMissingOffset ? \"\" : &kStringBlob[offset];",
		"}",
	]


def generate_files(model: LocalizationModel) -> GeneratedFiles:
	return GeneratedFiles(header=generate_header(model), cpp=generate_cpp(model))


def normalize_newlines(content: str) -> str:
	return content.replace("\r\n", "\n")


def write_if_changed(path: Path, content: str) -> bool:
	expected = normalize_newlines(content)
	existing = normalize_newlines(path.read_text(encoding="utf-8")) if path.exists() else None

	if existing == expected:
		return False

	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(expected, encoding="utf-8", newline="\n")
	return True


def check_file(path: Path, expected: str, show_diff: bool) -> bool:
	expected = normalize_newlines(expected)

	if not path.exists():
		print(f"out of date: {path} does not exist", file=sys.stderr)
		return False

	actual = normalize_newlines(path.read_text(encoding="utf-8"))
	if actual == expected:
		return True

	print(f"out of date: {path}", file=sys.stderr)
	if show_diff:
		print_diff(path, actual, expected)
	return False


def print_diff(path: Path, actual: str, expected: str) -> None:
	for line in difflib.unified_diff(
		actual.splitlines(keepends=True),
		expected.splitlines(keepends=True),
		fromfile=f"{path} (current)",
		tofile=f"{path} (generated)",
	):
		print(line, end="", file=sys.stderr)


def build_arg_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description="Generate RSVP Nano localization C++ from TOML.")
	parser.add_argument("--toml", type=Path, default=DEFAULT_TOML)
	parser.add_argument("--header", type=Path, default=DEFAULT_HEADER)
	parser.add_argument("--cpp", type=Path, default=DEFAULT_CPP)
	parser.add_argument("--check", action="store_true", help="fail if generated files are not up to date")
	parser.add_argument("--diff", action="store_true", help="print unified diffs when used with --check")
	return parser


def write_outputs(files: GeneratedFiles, header_path: Path, cpp_path: Path) -> None:
	for path, content in ((header_path, files.header), (cpp_path, files.cpp)):
		changed = write_if_changed(path, content)
		print(("wrote" if changed else "unchanged") + f" {path}")


def check_outputs(files: GeneratedFiles, header_path: Path, cpp_path: Path, show_diff: bool) -> bool:
	return all(
		(
			check_file(header_path, files.header, show_diff),
			check_file(cpp_path, files.cpp, show_diff),
		)
	)


def main() -> int:
	args = build_arg_parser().parse_args()

	try:
		files = generate_files(load_model(args.toml))

		if args.check:
			return 0 if check_outputs(files, args.header, args.cpp, args.diff) else 1

		write_outputs(files, args.header, args.cpp)
		return 0

	except Exception as exc:
		print(f"error: {exc}", file=sys.stderr)
		return 1


if __name__ == "__main__":
	raise SystemExit(main())
