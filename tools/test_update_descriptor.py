"""Release metadata must not label a Rev1 application as Rev2 (or vice versa)."""
import hashlib
from pathlib import Path
import tempfile
import unittest

from make_update_descriptor import BOARDS, descriptor


class DescriptorTests(unittest.TestCase):
    tag = "v0.2.0-rev2.1"
    commit = "a" * 40

    def image(self, path, board, *, dirty=False, merged=False):
        data = bytearray(65536)
        data[0] = 0xFF if merged else 0xE9
        version = f"{self.tag}+{self.commit[:12]}" + (".dirty" if dirty else "")
        identity = (version + "\0" + BOARDS[board][1] + "\0").encode()
        data[512:512+len(identity)] = identity
        path.write_bytes(data)

    def test_board_identity_and_hash(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            for board, (board_id, asset) in BOARDS.items():
                self.image(path, board)
                entry = descriptor(path, "new-ui", self.tag, self.commit, board)["releases"][0]
                self.assertEqual((entry["board"], entry["asset"]), (board_id, asset))
                self.assertEqual(entry["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
                self.assertEqual(entry["bytes"], path.stat().st_size)

    def test_wrong_revision(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            for board, wrong in (("lcd349-v1", "lcd349-v2"), ("lcd349-v2", "lcd349-v1")):
                self.image(path, board)
                with self.assertRaisesRegex(ValueError, "asset identity"):
                    descriptor(path, "new-ui", self.tag, self.commit, wrong)

    def test_non_release_images(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            for flags in ({"dirty": True}, {"merged": True}):
                self.image(path, "lcd349-v2", **flags)
                with self.assertRaises(ValueError):
                    descriptor(path, "new-ui", self.tag, self.commit, "lcd349-v2")

    def test_unsupported_board(self):
        with self.assertRaisesRegex(ValueError, "Unsupported board"):
            descriptor(Path("unused"), "new-ui", self.tag, self.commit, "unknown")


if __name__ == "__main__":
    unittest.main()
