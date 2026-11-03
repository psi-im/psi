"""Regression tests: released extensions are staged, not rebuilt by Psi."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[2] / "packaging/windows/stage_released_extensions.py"
SPEC = importlib.util.spec_from_file_location("psi_stage_released_extensions", SCRIPT)
staging = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(staging)


class ReleasedExtensionsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.psi_stage = self.root / "psi"
        self.psi_stage.mkdir()
        self.plugins = self.root / "plugins"
        self.plugins.mkdir()
        self.media = self.root / "media"
        # Match the published PsiMedia archive layout: the media plugin is
        # installed under psi/, while its GStreamer runtime is at stage root.
        self.media_stage = self.media / "stage-msvc-modern"
        self.media_payload = self.media_stage / "psi"
        (self.media_payload / "plugins").mkdir(parents=True)
        (self.media_stage / "gstreamer-1.0").mkdir()
        (self.media_payload / "plugins" / "mediaplugin.dll").write_bytes(b"media")
        (self.media_stage / "gstreamer-1.0" / "gstcore.dll").write_bytes(b"gstreamer")
        (self.media_stage / "gstreamer-runtime.txt").write_text("elements installed\n")
        self.lock = {
            "qt": {"windows_msvc_qt6": "6.12.0"},
            "plugins": {"release_tag": "1.6"},
            "psimedia": {"repository": "psi-im/psimedia", "release_tag": "v1.2.1",
                         "commit": "b" * 40},
        }
        self.lock_path = self.root / "dependencies.lock.json"
        self.lock_path.write_text(json.dumps(self.lock))
        self.iris_lock = self.root / "iris" / "dependencies.lock.json"
        self.iris_lock.parent.mkdir()
        self.iris_lock.write_text(json.dumps({"qca": {"tag": "v3.0.12", "commit": "c" * 40}}))
        files = {}
        for i in range(24):
            name = f"plugins/official-{i:02}.dll"
            path = self.plugins / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(f"binary-{i}".encode())
            files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        self.manifest = {
            "platform": "windows-x64", "qt": "6.12.0",
            "plugins_commit": "a" * 40, "psi_api_commit": "d" * 40,
            "qca_tag": "v3.0.12", "qca_commit": "c" * 40,
            "files": files,
        }
        self.write_manifest()

    def write_manifest(self):
        (self.plugins / "PLUGINS-MANIFEST.json").write_text(json.dumps(self.manifest))

    def stage(self):
        staging.stage_extensions(self.psi_stage, self.plugins, self.media, self.lock_path)

    def test_stages_all_pinned_binaries_and_runtime(self):
        self.stage()
        self.assertTrue((self.psi_stage / "plugins" / "official-00.dll").is_file())
        self.assertTrue((self.psi_stage / "plugins" / "mediaplugin.dll").is_file())
        self.assertFalse((self.psi_stage / "psi").exists())
        self.assertTrue((self.psi_stage / "gstreamer-1.0" / "gstcore.dll").is_file())
        self.assertTrue((self.psi_stage / "PLUGINS-MANIFEST.json").is_file())
        source = json.loads((self.psi_stage / "PSIMEDIA-SOURCE.json").read_text())
        self.assertEqual(source["commit"], "b" * 40)
        self.assertEqual(source["qt"], "6.12.0")
        # PsiMedia's original directory structure is retained in provenance,
        # while both independent installation roots are merged into Psi.
        self.assertIn("psi/plugins/mediaplugin.dll", source["files"])
        self.assertIn("gstreamer-1.0/gstcore.dll", source["files"])
        self.assertIn("gstreamer-runtime.txt", source["files"])
        self.assertFalse((self.psi_stage / "psi").exists())

    def test_refuses_tampered_official_plugin(self):
        (self.plugins / "plugins" / "official-00.dll").write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            self.stage()

    def test_refuses_a_checkout_without_the_iris_lock(self):
        self.iris_lock.unlink()
        with self.assertRaisesRegex(ValueError, "dependency lock of the pinned Iris"):
            staging.stage_extensions(self.psi_stage, self.plugins, self.media, self.lock_path)

    def test_refuses_qca_tag_mismatch(self):
        self.manifest["qca_tag"] = "v3.0.11"
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, "qca_tag mismatch"):
            self.stage()

    def test_accepts_moved_source_shas_for_same_release_tag(self):
        # Git release tags can move; artifact hashes still secure the payload.
        self.manifest["plugins_commit"] = "e" * 40
        self.manifest["psi_api_commit"] = "f" * 40
        self.manifest["qca_commit"] = "1" * 40
        self.write_manifest()
        self.stage()
        self.assertTrue((self.psi_stage / "plugins" / "official-00.dll").is_file())

    def test_refuses_wrong_qt(self):
        self.manifest["qt"] = "6.11.0"
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, "qt mismatch"):
            self.stage()

    def test_refuses_overwriting_psi_runtime_or_other_plugins(self):
        original = self.psi_stage / "plugins" / "official-00.dll"
        original.parent.mkdir()
        original.write_bytes(b"unexpected old plugin")
        with self.assertRaisesRegex(ValueError, "Conflicting"):
            self.stage()

    def test_refuses_absent_gstreamer_runtime(self):
        (self.media_stage / "gstreamer-runtime.txt").unlink()
        with self.assertRaisesRegex(ValueError, "runtime manifest missing"):
            self.stage()


if __name__ == "__main__":
    unittest.main()
