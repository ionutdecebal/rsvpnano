"""Exercise legacy discovery and the actual package-to-installer staging path."""
import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import check_legacy_ota_metadata as legacy
import export_web_firmware as firmware
import fetch_release_firmware as fetch
import package_release as package


class ReleasePackaging(unittest.TestCase):
    def make_images(self, directory):
        ota_by_env = {entry["env"]: entry["binary"] for entry in firmware.OTA_EXPORTS}
        for index, entry in enumerate(firmware.FLASH_EXPORTS):
            ota = b"\xe9" + bytes([index]) * 100
            (directory / ota_by_env[entry["env"]]).write_bytes(ota)
            (directory / entry["binary"]).write_bytes(b"\xe9" + b"\xff" * 65535 + ota)

    def test_package_aliases_are_exact_and_browser_bundle_stages_all_boards(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "source"; source.mkdir()
            staged = Path(folder) / "staged"; staged.mkdir()
            self.make_images(source)
            package.package_release(source)
            assets = package.published_assets(source)
            self.assertEqual(13, len(assets))  # 9 OTA, 2 aliases, ZIP, checksums
            self.assertTrue(all(path.exists() for path in assets))
            for alias, canonical in package.LEGACY_OTA_ALIASES.items():
                self.assertEqual((source / alias).read_bytes(), (source / canonical).read_bytes())
            release = {"assets": [{"name":p.name,"browser_download_url":p.name} for p in assets]}
            names = [entry["binary"] for entry in firmware.FLASH_EXPORTS]
            def download(url, destination):
                destination.write_bytes((source / url).read_bytes())
            with patch.object(fetch, "FIRMWARE_DIR", staged), patch.object(fetch, "download_file", side_effect=download):
                self.assertEqual(set(names), fetch.stage_browser_bundle(release, names))
            for name in names:
                self.assertEqual((source / name).read_bytes(), (staged / name).read_bytes())

    def test_cross_board_or_merged_ota_is_rejected_before_packaging(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); self.make_images(root)
            aliases = list(package.LEGACY_OTA_ALIASES.values())
            (root / aliases[0]).write_bytes((root / aliases[1]).read_bytes())
            with self.assertRaisesRegex(SystemExit, "Merged/OTA"):
                package.package_release(root)
            self.assertFalse((root / package.BROWSER_BUNDLE).exists())

    def test_bad_member_preserves_all_previously_staged_images(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "source"; source.mkdir()
            staged = Path(folder) / "staged"; staged.mkdir()
            self.make_images(source); package.package_release(source)
            names = [entry["binary"] for entry in firmware.FLASH_EXPORTS]
            for name in names:
                (staged / name).write_bytes(b"previous")
            sums = source / "SHA256SUMS.txt"
            text = sums.read_text()
            digest = hashlib.sha256((source / names[-1]).read_bytes()).hexdigest()
            sums.write_text(text.replace(digest + "  " + names[-1], "0" * 64 + "  " + names[-1]))
            release = {"assets": [{"name":p.name,"browser_download_url":p.name} for p in package.published_assets(source)]}
            with patch.object(fetch, "FIRMWARE_DIR", staged), patch.object(fetch, "download_file", side_effect=lambda url,path:path.write_bytes((source / url).read_bytes())):
                with self.assertRaisesRegex(SystemExit, "checksum mismatch"):
                    fetch.stage_browser_bundle(release, names)
            self.assertTrue(all((staged / name).read_bytes() == b"previous" for name in names))
            self.assertEqual(set(names), {path.name for path in staged.iterdir()})

    def test_existing_individual_downloads_do_not_require_a_bundle(self):
        name = firmware.FLASH_EXPORTS[0]["binary"]
        with patch.object(fetch, "download_file", side_effect=AssertionError("unexpected download")):
            self.assertEqual(set(), fetch.stage_browser_bundle({"assets":[{"name":name}]}, [name]))

    def metadata(self):
        return {"tag_name":"v0.1.4","assets":[
            {"name":name,"size":100000,"digest":"sha256:"+str(index)*64,
             "browser_download_url":"https://github.com/example/releases/download/v0.1.4/"+name}
            for index,(alias,canonical) in enumerate(package.LEGACY_OTA_ALIASES.items())
            for name in (alias,canonical)
        ]}

    def test_old_metadata_limit_and_matching_alias_bytes_are_enforced(self):
        document = self.metadata()
        legacy.check_metadata(json.dumps(document).encode())
        document["assets"][0]["size"] += 1
        with self.assertRaisesRegex(SystemExit, "does not match"):
            legacy.check_metadata(json.dumps(document).encode())
        document = self.metadata()
        document["assets"].insert(0, {"name":"padding","label":"x" * 32768})
        with self.assertRaisesRegex(SystemExit, "outside the first"):
            legacy.check_metadata(json.dumps(document).encode())


if __name__ == "__main__":
    unittest.main()
