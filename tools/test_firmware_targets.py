"""Keep the board layouts and the CI/release inventory in sync."""
from __future__ import annotations

import configparser
import fnmatch
import json
import re
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import export_web_firmware as firmware

ROOT = Path(__file__).resolve().parents[1]

# Physical size/form-factor audit and vendor references: docs/ui-layouts.md.
EXPECTED_LAYOUTS = {
    "waveshare_esp32s3_touch_lcd_349_rev1": "regular",
    "waveshare_esp32s3_touch_lcd_349_rev2": "regular",
    "waveshare_esp32s3_touch_amoled_18_v1": "watch",
    "waveshare_esp32s3_touch_amoled_18_v2": "watch",
    "waveshare_esp32s3_touch_amoled_206": "watch",
    "waveshare_esp32s3_touch_amoled_216": "regular",
    "waveshare_esp32s3_touch_amoled_241": "regular",
    "waveshare_esp32s3_touch_amoled_241_v2": "regular",
    "waveshare_esp32c6_touch_lcd_147": "watch",
}


class FirmwareTargetsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.config = configparser.ConfigParser(interpolation=None)
        self.config.read(ROOT / "platformio.ini", encoding="utf-8")

    def option(self, section: str, name: str) -> str:
        if self.config.has_option(section, name):
            value = self.config.get(section, name)
        elif self.config.has_option(section, "extends"):
            return self.option(self.config.get(section, "extends"), name)
        else:
            return self.option("env", name)
        return re.sub(
            r"\$\{([^}]+)\}",
            lambda match: self.option(*match[1].rsplit(".", 1)),
            value,
        )

    def test_every_production_environment_is_audited_and_exported(self) -> None:
        environments = {
            section.removeprefix("env:") for section in self.config.sections()
            if section.startswith("env:waveshare_")
        }
        self.assertEqual(set(EXPECTED_LAYOUTS), environments)
        self.assertEqual(environments, set(firmware.REQUIRED_ENVS))
        self.assertEqual(environments, {entry["env"] for entry in firmware.FLASH_EXPORTS})
        self.assertEqual(environments, {entry["env"] for entry in firmware.OTA_EXPORTS})

    def test_only_the_expected_layout_is_compiled(self) -> None:
        for env, expected in EXPECTED_LAYOUTS.items():
            sections = [f"env:{env}"]
            benchmark = f"env:benchmark_{env}"
            if self.config.has_section(benchmark):
                sections.append(benchmark)
            for section in sections:
                filters = re.findall(r"([+-])<([^>]+)>", self.option(section, "build_src_filter"))
                for layout in ("regular", "watch"):
                    for screen in ("ReadScreen.cpp", "SettingsScreen.cpp", "ReaderLayout.cpp"):
                        path = f"ui/screens/{layout}/{screen}"
                        selected = False
                        for sign, pattern in filters:
                            if fnmatch.fnmatchcase(path, pattern):
                                selected = sign == "+"
                        with self.subTest(env=section, path=path):
                            self.assertEqual(layout == expected, selected)

    def test_release_names_are_unique_and_keep_v1_compatibility(self) -> None:
        for exports in (firmware.FLASH_EXPORTS, firmware.OTA_EXPORTS):
            self.assertEqual(len(exports), len({entry["env"] for entry in exports}))
        all_binaries = [entry["binary"] for entry in (*firmware.FLASH_EXPORTS, *firmware.OTA_EXPORTS)]
        self.assertEqual(len(all_binaries), len(set(all_binaries)))
        by_id = {entry["id"]: entry for entry in firmware.FLASH_EXPORTS}
        self.assertEqual(len(by_id), len(firmware.FLASH_EXPORTS))
        self.assertEqual("rsvp-nano-esp32-s3-touch-amoled-2.41.bin", by_id["amoled241"]["binary"])
        self.assertEqual("rsvp-nano-esp32-s3-touch-amoled-2.41-v2.bin", by_id["amoled241-v2"]["binary"])
        for version, env in ((1, "waveshare_esp32s3_touch_amoled_241"), (2, "waveshare_esp32s3_touch_amoled_241_v2")):
            header = f"platforms/waveshare_amoled_241/v{version}/WaveshareAmoled241Version.h"
            self.assertIn(header, self.option(f"env:{env}", "build_flags"))
            self.assertIn(header, self.option(f"env:native_amoled_241_v{version}_test", "build_flags"))
            ota = next(entry["binary"] for entry in firmware.OTA_EXPORTS if entry["env"] == env)
            self.assertIn(f'"{ota}"', (ROOT / "src" / header).read_text(encoding="utf-8"))

    def test_release_assembly_requires_v2_and_publishes_its_installer(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            versions = directory / "versions"
            versions.mkdir()
            for env in firmware.REQUIRED_ENVS:
                (versions / f"{env}.txt").write_text("test-version\n", encoding="utf-8")
            for entry in (*firmware.FLASH_EXPORTS, *firmware.OTA_EXPORTS):
                (directory / entry["binary"]).write_bytes(b"test fixture")
            v2_ota = directory / "rsvp-nano-esp32-s3-touch-amoled-2.41-v2-ota.bin"
            v2_ota.unlink()
            metadata = directory / "release.json"
            with patch.multiple(firmware, FIRMWARE_DIR=directory, VERSION_DIR=versions, RELEASE_METADATA_PATH=metadata):
                with self.assertRaisesRegex(SystemExit, "2.41-v2-ota.bin"):
                    firmware.assemble_release()
                v2_ota.write_bytes(b"test fixture")
                firmware.assemble_release()
            release = json.loads(metadata.read_text(encoding="utf-8"))
            self.assertEqual("test-version", release["version"])
            self.assertEqual("rsvp-nano-esp32-s3-touch-amoled-2.41-v2.bin", release["firmware"]["amoled241-v2"])
            self.assertEqual(9, len(release["firmware"]))


if __name__ == "__main__":
    unittest.main()
