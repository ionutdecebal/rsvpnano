import hashlib, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import fetch_release_firmware as fetch

class Channels(unittest.TestCase):
    def test_preview_is_separate_and_verified(self):
        name="rsvp-nano-esp32-s3-touch-lcd-3.49.bin"
        tag="preview-ui-dictionary-20261020.1"
        blob=b"candidate"
        release={"tag_name":tag,"prerelease":True,"draft":False,
                 "assets":[{"name":name,"browser_download_url":"image"},
                           {"name":"SHA256SUMS.txt","browser_download_url":"sums"}]}
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/name).write_bytes(b"classic")
            def download(url,path):
                path.write_bytes(blob if url=="image" else f"{hashlib.sha256(blob).hexdigest()}  {name}\n".encode())
            with patch.object(fetch,"FIRMWARE_DIR",root),patch.object(fetch,"fetch_json",return_value=release),patch.object(fetch,"download_file",side_effect=download):
                result=fetch.stage_ui_preview("owner/repo",tag)
                self.assertEqual((root/name).read_bytes(),b"classic")
                self.assertEqual(result["firmware"],{"lcd349-v1":"new-ui-"+name})
                self.assertEqual((root/("new-ui-"+name)).read_bytes(),blob)
                with self.assertRaises(SystemExit):fetch.stage_ui_preview("owner/repo","latest")
                release["prerelease"]=False
                with self.assertRaises(SystemExit):fetch.stage_ui_preview("owner/repo",tag)

    def test_bad_checksum_cannot_replace_existing_preview(self):
        name="rsvp-nano-esp32-s3-touch-lcd-3.49.bin";tag="preview-ui-dictionary-next"
        release={"tag_name":tag,"prerelease":True,"assets":[{"name":name,"browser_download_url":"image"},{"name":"SHA256SUMS.txt","browser_download_url":"sums"}]}
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/("new-ui-"+name);dest.write_bytes(b"previous")
            with patch.object(fetch,"FIRMWARE_DIR",root),patch.object(fetch,"fetch_json",return_value=release),patch.object(fetch,"download_file",side_effect=lambda url,path:path.write_bytes(b"bad")):
                with self.assertRaises(SystemExit):fetch.stage_ui_preview("owner/repo",tag)
                self.assertEqual(dest.read_bytes(),b"previous")
                self.assertFalse(list(root.glob("*.partial")))
if __name__=="__main__":unittest.main()
