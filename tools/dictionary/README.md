# Offline English pack

`build_dictionary.py SOURCE_JSON_ZIP OUTPUT_DIRECTORY` converts the official
Open English Wordnet 2025 JSON release to the device's bounded indexed format.
It writes `english.oewn`, `manifest.json` and `ATTRIBUTION.txt`; no server is needed.

Release input SHA-256:
`7d749f6e2c39e6970e4997839dcf6e42fd281f3c2fae0171d2192bae8cfa4b51`.

Expected output SHA-256:
`9f893776ba6b9b5f93abae7c06cdc5ff90ebf7cb4427d343160f26b1488ad9c6`.

The manifest records the source, licence, transformations, counts, size and both
hashes. Check the pack with `python3 test/native_dictionary/run.py OUTPUT_DIRECTORY/english.oewn`.
Distribute the pack with attribution, as documented in the
[release guide](../../docs/releases/ui-dictionary-preview.md).
