import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch, Mock
from types import SimpleNamespace

from run import (
    DEVICE_EPUB_PATH,
    DEVICE_MULTILINGUAL_PATH,
    DEVICE_VERTICAL_EPUB_PATH,
    DEVICE_MARKER_PATH,
    FONT_MANIFEST_PATH,
    RUN_READY_PATH,
    device_serial,
    find_benchmark_volume,
    monitor_command,
    prepare_volume,
    select_serial_port,
    prepare_reader_volume,
    DEVICE_READER_FIXTURE,
    platformio_command,
    reader_monitor_command,
)


class BenchmarkRunTests(unittest.TestCase):
    def test_prepares_only_marked_volume_and_writes_ready_last(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            volume = root / "volume"
            other = root / "other"
            (volume / "benchmark").mkdir(parents=True)
            other.mkdir()
            (volume / "benchmark/.rsvpnano-benchmark").write_text("device\n", encoding="ascii")
            fixture = root / "fixture.epub"
            fixture.write_bytes(b"epub")
            multilingual = root / "multilingual.rsvp"
            multilingual.write_bytes(b"@rsvp 1\nmultilingual")
            vertical = root / "vertical.epub"
            vertical.write_bytes(b"vertical")
            fonts = root / "fonts"
            (fonts / "Reader Font").mkdir(parents=True)
            (fonts / "Reader Font/font.rfont4").write_bytes(b"font-v6")
            (fonts / "index.json").write_text(
                '[{"id":"reader-font","file":"Reader Font/font.rfont4"}]', encoding="utf-8"
            )
            legacy_font = volume / "fonts/Reader Font/font.rfont4"
            legacy_font.parent.mkdir(parents=True)
            legacy_font.write_bytes(b"font-v5")

            self.assertEqual(volume, find_benchmark_volume([other, volume]))
            prepare_volume(volume, fixture, multilingual, vertical, fonts)

            self.assertEqual(b"epub", (volume / DEVICE_EPUB_PATH).read_bytes())
            self.assertEqual(b"@rsvp 1\nmultilingual", (volume / DEVICE_MULTILINGUAL_PATH).read_bytes())
            self.assertEqual(b"vertical", (volume / DEVICE_VERTICAL_EPUB_PATH).read_bytes())
            self.assertEqual(b"font-v6", (volume / "fonts/reader-font/font.rfont4").read_bytes())
            self.assertFalse(legacy_font.exists())
            self.assertIn("reader-font/font.rfont4", (volume / FONT_MANIFEST_PATH).read_text(encoding="ascii"))
            self.assertEqual("ready\n", (volume / RUN_READY_PATH).read_text(encoding="ascii"))

            installed_font = volume / "fonts/reader-font/font.rfont4"
            installed_mtime = installed_font.stat().st_mtime_ns
            (volume / FONT_MANIFEST_PATH).unlink()
            prepare_volume(volume, fixture, multilingual, vertical, fonts)
            self.assertEqual(installed_mtime, installed_font.stat().st_mtime_ns)

    def test_does_not_rewrite_unchanged_fixtures(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / "fixture.epub"
            fixture.write_bytes(b"epub")
            multilingual = root / "multilingual.rsvp"
            multilingual.write_bytes(b"@rsvp 1\ntext")

            prepare_volume(root, fixture, multilingual)
            epub_mtime = (root / DEVICE_EPUB_PATH).stat().st_mtime_ns
            multilingual_mtime = (root / DEVICE_MULTILINGUAL_PATH).stat().st_mtime_ns
            prepare_volume(root, fixture, multilingual)

            self.assertEqual(epub_mtime, (root / DEVICE_EPUB_PATH).stat().st_mtime_ns)
            self.assertEqual(multilingual_mtime, (root / DEVICE_MULTILINGUAL_PATH).stat().st_mtime_ns)

    def test_tracks_device_across_com_port_change(self) -> None:
        devices = [{"port": "COM7", "hwid": "USB VID:PID=303A:1001 SER=44:1B:F6:85:56:E0"}]
        serial = device_serial({"port": "COM3", "hwid": "USB VID:PID=303A:1001 SER=441BF68556E0"})
        self.assertEqual("COM7", select_serial_port(devices, requested="COM3", serial=serial))

    def test_monitor_runs_in_the_platformio_python_process(self) -> None:
        command = monitor_command("python", "benchmark_env", "COM3", "115200")

        self.assertEqual(["python", "-m", "platformio", "device", "monitor"], command[:5])
        self.assertNotIn("uvx", command)

    def test_reader_profile_writes_unique_words_and_preserves_books_fonts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(RuntimeError, "Not an RSVP"):
                prepare_reader_volume(root)
            (root / DEVICE_MARKER_PATH).parent.mkdir(parents=True)
            (root / DEVICE_MARKER_PATH).write_text("fixture")
            existing = root / "fonts/reader/font.rfont4"
            existing.parent.mkdir(parents=True); existing.write_bytes(b"keep-font")
            book = root / "library/books/keep.rsvp"
            book.parent.mkdir(parents=True); book.write_bytes(b"keep-book")
            prepare_reader_volume(root)
            lines = (root / DEVICE_READER_FIXTURE).read_text().splitlines()
            words = " ".join(line for line in lines if not line.startswith("@")).split()
            self.assertEqual(1000, len(words)); self.assertEqual(1000, len(set(words)))
            self.assertTrue(words[0].endswith("0001")); self.assertTrue(words[-1].endswith("1000"))
            self.assertEqual("reader-only\n", (root / RUN_READY_PATH).read_text())
            self.assertEqual(b"keep-font", existing.read_bytes()); self.assertEqual(b"keep-book", book.read_bytes())

    def test_explicit_missing_port_does_not_upload_to_another_board(self) -> None:
        self.assertEqual("", select_serial_port([{"port":"COM7", "hwid":"USB VID:PID=303A:1001"}], requested="COM3"))

    def test_reuses_installed_platformio_without_uvx(self) -> None:
        with patch("run.shutil.which", side_effect=lambda name: "/installed/pio" if name == "pio" else None):
            self.assertEqual(["/installed/pio"], platformio_command())

    def test_reader_monitor_sends_start_on_the_same_connection(self) -> None:
        command = reader_monitor_command("python", "COM3", "115200")
        self.assertEqual(["python", "-u", "-c"], command[:3])
        compile(command[3], "reader-monitor", "exec")
        self.assertIn("reader-start\\n", command[3])
        self.assertEqual(["COM3", "115200"], command[-2:])

    def test_reader_monitor_handles_a_fragmented_start_banner(self) -> None:
        command = reader_monitor_command("python", "COM3", "115200")
        port = Mock()
        port.readline.side_effect = [b"[bench] sta", b"rt board=Test\n", b"metrics\n", b"[bench] done\n"]
        with patch.dict("sys.modules", {"serial": SimpleNamespace(Serial=lambda: port)}), \
             patch("sys.argv", ["monitor", "COM3", "115200"]), \
             patch("time.monotonic", side_effect=[2, 2, 4, 4]), patch("builtins.print"):
            exec(compile(command[3], "reader-monitor", "exec"), {})
        self.assertEqual(2, port.write.call_count)
        port.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
