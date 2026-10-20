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

    def test_public_020_preview_tag_is_allowed(self):
        with patch.object(fetch, "fetch_json", side_effect=RuntimeError("validated tag")):
            with self.assertRaisesRegex(RuntimeError, "validated tag"):
                fetch.stage_ui_preview("owner/repo", "v0.2.0")
        for tag in ("latest", "../v0.2.0", "v0.1.2", "main"):
            with self.assertRaises(SystemExit):
                fetch.stage_ui_preview("owner/repo", tag)

    def test_catalog_requires_matching_ota_bytes_and_preserves_old_on_error(self):
        import json
        name="rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin"
        commit="a"*40; tag="v0.2.0"
        image=b"\xe9"+(tag+"+"+commit[:12]).encode()+b"\0"+b"x"*65536
        entry={"channel":"new-ui","board":"waveshare_esp32s3_touch_lcd_3_49",
               "tag":tag,"commit":commit,"asset":name,"bytes":len(image),
               "sha256":hashlib.sha256(image).hexdigest()}
        descriptor={"schema":1,"releases":[entry]}
        preview={"tag_name":tag,"prerelease":True,"assets":[
            {"name":"update-channel.json","browser_download_url":"descriptor"},
            {"name":name,"browser_download_url":"ota"}]}
        classic={"tag_name":"v0.1.2","assets":[]} # Old Classic is not a switch target.
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def download(url,path):
                path.write_bytes(json.dumps(descriptor).encode() if url=="descriptor" else image)
            with patch.object(fetch,"FIRMWARE_DIR",root),patch.object(fetch,"fetch_json",return_value=preview),patch.object(fetch,"download_file",side_effect=download):
                fetch.stage_update_catalog("owner/repo",classic,tag)
                self.assertEqual(json.loads((root/"channels.json").read_text())["releases"],[entry])
                original=(root/"channels.json").read_bytes()
                entry["sha256"]="0"*64
                with self.assertRaises(SystemExit):fetch.stage_update_catalog("owner/repo",classic,tag)
                self.assertEqual((root/"channels.json").read_bytes(),original)
                self.assertFalse(list(root.glob("*.partial")))
if __name__=="__main__":unittest.main()
