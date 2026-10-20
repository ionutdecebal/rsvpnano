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

## GitHub data release

English is the only offered language for now. More languages are planned.
The device downloads raw assets from the independent `dictionary-en-2025.1`
release in `ionutdecebal/rsvpnano`; no firmware release or personal server is needed.

Prepare the assets from a verified existing pack:

```sh
python3 tools/dictionary/package_release.py PACK_DIRECTORY NEW_ASSET_DIRECTORY
python3 test/native_dictionary/run.py NEW_ASSET_DIRECTORY/english.oewn
```

`english-2025.1.json` pins the source and pack identity. The packager verifies
bytes, SHA-256, header and source manifest, then includes provenance and notices.
The raw `english.oewn`, `manifest.json` and `ATTRIBUTION.txt` are downloaded by the
device. The ZIP installs those files plus licence texts under `/dictionaries`.
Both manifest and attribution must fit the device's 4096-byte download bounds.

Publish all generated assets as a **data-only prerelease**, explicitly not Latest,
using the exact source commit and tag `dictionary-en-2025.1`. The current Classic
firmware release must remain the latest release. Verify anonymous HTTPS downloads
and hashes after publication. Never clobber published assets; increment the pack
version for changes. Firmware acceptance remains separate from data publication.

Upstream licence files under `licenses/` are preserved from:
https://github.com/globalwordnet/english-wordnet/tree/2025-edition
