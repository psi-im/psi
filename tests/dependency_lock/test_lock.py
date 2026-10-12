"""Regression tests for Psi's dependency lock and CI integration."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "psi_dependency_resolver", ROOT / ".github/scripts/resolve-dependencies.py")
resolver = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(resolver)


class DependencyLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.lock = json.loads((ROOT / "dependencies.lock.json").read_text())
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))

    def with_iris(self, tag="v3.0.11", minimum="3.0.10"):
        iris_dir = self.root / "iris"
        iris_dir.mkdir()
        (iris_dir / "dependencies.lock.json").write_text(json.dumps({
            "qca": {
                "tag": tag,
                "commit": "a" * 40,
                "minimum_version": minimum,
            }
        }))

    def test_resolves_windows_and_psimedia_without_iris_checkout(self):
        values = resolver.read_dependencies(self.root)
        self.assertEqual(values["PSIMEDIA_SRTP_REF"], self.lock["psimedia"]["commit"])
        self.assertEqual(values["QTKEYCHAIN_COMMIT"], self.lock["qtkeychain"]["commit"])
        self.assertEqual(values["MSVC_QT6_SDK_TAG"], self.lock["windows_sdks"]["msvc_qt6"]["tag"])
        self.assertNotIn("QCA_TAG", values)

    def test_inherits_qca_from_iris_without_second_pin(self):
        self.assertNotIn("qca", self.lock)
        self.with_iris()
        values = resolver.read_dependencies(self.root, require_iris=True)
        self.assertEqual(values["QCA_TAG"], "v3.0.11")
        self.assertEqual(values["QCA_MIN_VERSION"], "3.0.10")

    def test_rejects_missing_iris_and_older_qca_than_minimum(self):
        with self.assertRaises(ValueError):
            resolver.read_dependencies(self.root, require_iris=True)
        self.with_iris(tag="v3.0.9")
        with self.assertRaises(ValueError):
            resolver.read_dependencies(self.root, require_iris=True)

    def test_rejects_bad_revision_and_archive_path(self):
        self.lock["psimedia"]["commit"] = "master"
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        with self.assertRaises(ValueError):
            resolver.read_dependencies(self.root)
        self.lock["psimedia"]["commit"] = "b" * 40
        self.lock["windows_sdks"]["msys2_qt5"]["archive"] = "../exploit.zip"
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        with self.assertRaises(ValueError):
            resolver.read_dependencies(self.root)

    def test_dependency_pins_are_not_duplicated_in_workflows(self):
        workflow_dir = ROOT / ".github/workflows"
        pins = [
            self.lock["psimedia"]["commit"],
            self.lock["qtkeychain"]["tag"],
            self.lock["windows_sdks"]["msvc_qt6"]["tag"],
            self.lock["windows_sdks"]["msys2_qt5"]["tag"],
        ]
        for workflow in workflow_dir.glob("*.yml"):
            content = workflow.read_text()
            for pin in pins:
                self.assertNotIn(pin, content, f"Hardcoded pin {pin} in {workflow}")
            self.assertNotIn("QCA_TAG: v3.", content, str(workflow))

    def test_resolved_consumers(self):
        workflows = {
            "psimedia-integration.yml": "./.github/actions/load-dependencies",
            "windows-sdk-msvc-qt6.yml": "./.github/actions/load-dependencies",
            "windows-sdk-msys2-qt5.yml": "./.github/actions/load-dependencies",
            "windows-msvc-qt6.yml": "./.github/actions/load-dependencies",
            "windows-msys2-qt5.yml": "./.github/actions/load-dependencies",
            "ai-windows-gcc-basic.yml": "./.github/actions/load-dependencies",
        }
        for path, needle in workflows.items():
            self.assertIn(needle, (ROOT / ".github/workflows" / path).read_text(), path)


if __name__ == "__main__":
    unittest.main()
