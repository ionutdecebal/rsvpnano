import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import fetch_release_firmware as fetch


class Rev2Channels(unittest.TestCase):
    def setUp(self):
        self.payloads = {}
        self.descriptors = {}
        self.releases = {}
        for channel, tag, boards in (
            ('classic', 'v0.1.5', ['lcd349-v1', 'lcd349-v2']),
            ('new-ui', 'v0.2.0', ['lcd349-v1']),
            ('new-ui', 'v0.2.0-rev2.1', ['lcd349-v2']),
        ):
            entries, assets = [], []
            for board in boards:
                board_id, name = fetch.UI_BOARDS[board]
                commit = 'a' * 40
                image = b'\xe9' + (tag + '+' + commit[:12]).encode() + b'\0' + name.encode() + b'\0' + b'x' * 65536
                entry = dict(channel=channel, board=board_id, tag=tag, commit=commit,
                             asset=name, bytes=len(image), sha256=hashlib.sha256(image).hexdigest())
                entries.append(entry)
                url = tag + '/' + name
                self.payloads[url] = image
                assets.append(dict(name=name, browser_download_url=url))
            descriptor_url = tag + '/descriptor'
            self.descriptors[descriptor_url] = dict(schema=1, releases=entries)
            assets.append(dict(name='update-channel.json', browser_download_url=descriptor_url))
            self.releases[tag] = dict(tag_name=tag, assets=assets, prerelease=channel == 'new-ui')

    def stage(self, root):
        def download(url, path):
            path.write_bytes(json.dumps(self.descriptors[url]).encode() if url in self.descriptors else self.payloads[url])
        with patch.object(fetch, 'FIRMWARE_DIR', root), patch.object(fetch, 'fetch_json', side_effect=lambda url: self.releases[url.rsplit('/', 1)[1]]), patch.object(fetch, 'download_file', side_effect=download):
            fetch.stage_update_catalog('owner/repo', self.releases['v0.1.5'], 'v0.2.0', 'v0.2.0-rev2.1')

    def test_four_targets_and_independent_versions(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.stage(root)
            entries = json.loads((root / 'channels.json').read_text())['releases']
            self.assertEqual(len(entries), 4)
            for short_board, (board, _) in fetch.UI_BOARDS.items():
                selected = {e['channel']: e['tag'] for e in entries if e['board'] == board}
                self.assertEqual(selected, {'classic': 'v0.1.5', 'new-ui': 'v0.2.0' if short_board == 'lcd349-v1' else 'v0.2.0-rev2.1'})

    def test_rejects_invalid_targets_without_replacing_live_catalog(self):
        for fault in ('wrong_board', 'duplicate', 'bad_hash', 'missing_return', 'missing_descriptor', 'wrong_binary_identity'):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as folder:
                self.setUp()
                root = Path(folder)
                self.stage(root)
                original = (root / 'channels.json').read_bytes()
                rev2 = self.descriptors['v0.2.0-rev2.1/descriptor']['releases'][0]
                if fault == 'wrong_board':
                    rev2['board'] = fetch.UI_BOARDS['lcd349-v1'][0]
                elif fault == 'duplicate':
                    entries = self.descriptors['v0.1.5/descriptor']['releases']
                    entries[1] = entries[0].copy()
                elif fault == 'bad_hash':
                    rev2['sha256'] = '0' * 64
                elif fault == 'missing_return':
                    self.descriptors['v0.1.5/descriptor']['releases'].pop()
                elif fault == 'missing_descriptor':
                    self.releases['v0.2.0-rev2.1']['assets'].pop()
                elif fault == 'wrong_binary_identity':
                    url = 'v0.2.0-rev2.1/' + rev2['asset']
                    data = self.payloads[url].replace(rev2['asset'].encode(), b'x' * len(rev2['asset']))
                    self.payloads[url] = data
                    rev2['sha256'] = hashlib.sha256(data).hexdigest()
                with self.assertRaises(SystemExit):
                    self.stage(root)
                self.assertEqual((root / 'channels.json').read_bytes(), original)
                self.assertFalse(list(root.glob('*.partial')))

    def test_rev2_browser_file_does_not_replace_rev1_or_classic(self):
        tag = 'v0.2.0-rev2.1'
        name = 'rsvp-nano-esp32-s3-touch-lcd-3.49-rev2.bin'
        blob = b'rev2 merged'
        release = dict(tag_name=tag, prerelease=True, assets=[
            dict(name=name, browser_download_url='image'),
            dict(name='SHA256SUMS.txt', browser_download_url='sums')])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / name).write_bytes(b'classic')
            rev1 = root / 'new-ui-rsvp-nano-esp32-s3-touch-lcd-3.49.bin'
            rev1.write_bytes(b'rev1')
            def download(url, path):
                path.write_bytes(blob if url == 'image' else f'{hashlib.sha256(blob).hexdigest()}  {name}\n'.encode())
            with patch.object(fetch, 'FIRMWARE_DIR', root), patch.object(fetch, 'fetch_json', return_value=release), patch.object(fetch, 'download_file', side_effect=download):
                result = fetch.stage_ui_preview('owner/repo', tag, 'lcd349-v2')
            self.assertEqual(result, {'version': tag, 'firmware': {'lcd349-v2': 'new-ui-' + name}})
            self.assertEqual((root / name).read_bytes(), b'classic')
            self.assertEqual(rev1.read_bytes(), b'rev1')
            self.assertEqual((root / ('new-ui-' + name)).read_bytes(), blob)


if __name__ == '__main__':
    unittest.main()
