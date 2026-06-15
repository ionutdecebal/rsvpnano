# RSVP Nano Conversion Specification

## Purpose

This specification defines how RSVP Nano converters transform supported source documents into
`.rsvp` files. All converter implementations must either conform to this specification or document
their limitations explicitly.

The authoritative behavior is verified with reference test cases under `testdata/conversion`.

## Output Format

Generated `.rsvp` files are UTF-8 text files with `\n` line endings.

Every generated file must start with:

```text
@rsvp 1
@title <title>
@source <source>
```

`@author <author>` should be included when the source provides a non-empty author.

Readable content follows the header. Lines are wrapped at 96 characters without splitting long
words. Readable lines that begin with `@` must be escaped by prefixing an additional `@`.

## Directives

| Directive | Required | Meaning |
| --- | --- | --- |
| `@rsvp 1` | Yes | Declares RSVP file format version 1. |
| `@title <value>` | Yes | Display title for the reader UI. |
| `@source <value>` | Yes | Original filename or URL. |
| `@author <value>` | No | Display author when known. |
| `@converter <value>` | No | Converter/cache identifier, mainly for firmware-generated cache files. |
| `@chapter <value>` | No | Chapter marker used for navigation and progress. |
| `@para` | No | Marks the next readable word as the start of a paragraph. |
| `@@...` | As needed | Escaped readable text line that starts with `@`. |

Converters should emit `@chapter` whenever reliable chapter structure is available. If no chapter
is available, converters should insert one fallback chapter based on the document title.

Converters should emit `@para` for each logical paragraph after readable text has already begun.
Chapter boundaries imply a paragraph boundary for the following readable text.

## Supported Inputs

### Convert To RSVP

- `.epub`
- `.txt`
- `.md`
- `.markdown`
- `.html`
- `.htm`
- `.xhtml`

### Pass Through

- `.rsvp`

### Firmware Fallback

The firmware can read or convert a narrower set directly:

- `.rsvp`
- `.txt`
- `.epub`

### Share And Draft Inputs

- URLs
- Plain text
- Text-like files: `.txt`, `.md`, `.markdown`, `.html`, `.htm`, `.xhtml`

## Text And Markdown Rules

- Decode text using the supported runtime decoder chain.
- Blank lines separate paragraphs.
- Markdown is treated as plain text with lightweight heading detection.
- Lines beginning with `#`, `Chapter`, `Part`, or `Book` are chapter candidates when the cleaned
  line is short enough to be a title.
- Full Markdown rendering is not currently part of the contract.

## HTML And XHTML Rules

- Ignore `head`, `math`, `nav`, `script`, `style`, and `svg`.
- Treat block elements as paragraph boundaries.
- Treat `h1` through `h6` as chapter markers.
- Decode named and numeric HTML entities.
- Collapse repeated whitespace inside each output paragraph.
- Preserve inline text order across nested spans and inline elements.

## EPUB Rules

- Read `META-INF/container.xml` to locate the OPF package file.
- Use OPF metadata title when present.
- Use OPF metadata creator as author when present.
- Build the manifest from OPF `item` entries.
- Prefer OPF spine reading order.
- Support both EPUB2 NCX and EPUB3 navigation documents.
- Prefer EPUB2 NCX labels for EPUB2 packages and EPUB3 navigation document labels for EPUB3
  packages. If the preferred TOC source is unavailable or empty, converters may fall back to the
  other TOC source.
- Resolve percent-encoded manifest, spine, TOC, and fragment references before matching them to ZIP
  entries or XHTML `id` / `name` anchors.
- Flatten nested TOC entries into reading order for `@chapter` output.
- Preserve multiple TOC entries that point to different anchors in the same content document; apply
  those labels in TOC order instead of collapsing them to one label per file.
- Ignore non-content TOC labels such as cover, title page, table of contents, and labels matching
  the book title.
- If the spine is missing or empty, a converter may fall back to manifest content documents.
- Content documents are selected by media type (`application/xhtml+xml`, `text/html`) or by
  `.xhtml`, `.html`, or `.htm` extension.
- Each selected content document is processed using the HTML/XHTML rules.
- If a readable content document is mapped by the EPUB TOC, the output chapter marker must use
  that TOC label and must not duplicate the document's first heading.
- If one readable content document contains several TOC-mapped chapters, generated `@chapter`
  markers should follow the ordered TOC labels and should replace short in-body headings such as
  bare Roman numerals.
- If an EPUB TOC is sparse and does not map readable body content documents, reliable in-body
  headings from those unmapped documents should still be preserved as chapter markers.
- If an EPUB TOC is available, readable content documents that are not mapped by the TOC must not
  receive generated filename chapter markers.
- Generated title pages and table-of-contents pages should not become chapter markers when a
  better TOC or body chapter structure is available.
- If no usable EPUB TOC is available and a readable content document has no heading, insert a
  chapter marker from its filename.
- EPUBs with `META-INF/encryption.xml` must fail with a clear unsupported-conversion error rather
  than attempting to emit partially readable or corrupt text.

## Text Normalization

Converters should normalize text consistently:

- Collapse repeated whitespace to a single space.
- Normalize common smart quotes, dashes, ellipses, non-breaking spaces, and ligatures.
- Preserve punctuation in output, including punctuation-only inline fragments.
- Count words using readable tokens containing at least one letter or digit.
- Preserve directive safety by escaping readable lines that begin with `@`.

## Encoding

Converters should support these inputs where the runtime allows it:

- UTF-8 with or without BOM.
- UTF-16 with BOM.
- UTF-16 XML-like documents without BOM.
- Windows-1252.
- ISO-8859-1 / Latin-1.

Unsupported or undecodable sources should fail with a clear conversion error rather than silently
producing empty output.

## Runtime Responsibilities

| Runtime | Role |
| --- | --- |
| Kotlin Multiplatform `:conversionCore` | Source implementation for Android, iOS, and generated JavaScript. |
| Generated web JavaScript | Product-facing browser converter built from `:conversionCore`. |
| Python tools | CLI and SD-card batch converter. |
| Firmware C++ | Constrained on-device fallback and RSVP parser. |

Runtime-specific APIs such as file pickers, storage, and firmware cache management may differ. The
Kotlin implementation should use shared ZIP and DOM parsing through `:conversionCore` where
possible. The resulting `.rsvp` output should still match the reference test cases unless a runtime
limitation is documented.

## Parity Requirements

- New converter behavior requires a reference test case.
- Reference test cases must cover representative text, Markdown, HTML/XHTML, EPUB, existing `.rsvp`
  pass-through, directive escaping, paragraphs, chapters, Unicode normalization, and punctuation
  preservation.
- Runnable converter implementations should be included in `tools/run_conversion_parity.py`.
- Output comparisons should normalize line endings only; content differences are failures.
