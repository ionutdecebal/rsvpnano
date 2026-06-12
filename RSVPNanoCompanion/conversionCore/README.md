# conversionCore (Kotlin Multiplatform)

This module contains the RSVP Nano document conversion engine.

It builds for Android, iOS, and JavaScript, and owns:

- `.rsvp`, EPUB, text, Markdown, HTML, and XHTML conversion.
- EPUB ZIP, OPF, NCX, EPUB3 navigation, and content parsing.
- Browser/Node exports used by the hosted web converter.
- Converter parity tests and fixtures.

Quick start:

```bash
bash ./gradlew :conversionCore:testDebugUnitTest :conversionCore:publishWebConverterJs
python RSVPNanoCompanion/tools/run_conversion_parity.py
```

The web converter artifact is generated into:

```text
web/generated/converter/rsvpnano_converter.mjs
```

That generated directory is intentionally ignored by Git. GitHub Pages builds it before uploading
the website artifact.
