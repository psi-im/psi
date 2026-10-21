"""Regression tests for Psi's dependency lock and CI integration."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


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
        # Most fixtures exercise the released-SDK contract independently of
        # whether the working development lock currently has a published SDK.
        self.lock["iris"]["release_tag"] = "v1.5.1"
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))

    def with_iris(self, tag="v3.0.12", minimum="3.0.10"):
        iris_dir = self.root / "iris"
        iris_dir.mkdir()
        (iris_dir / "dependencies.lock.json").write_text(json.dumps({
            "qca": {
                "tag": tag,
                "commit": self.lock["qca"]["commit"],
                "minimum_version": minimum,
            }
        }))

    def test_resolves_windows_and_psimedia_without_iris_checkout(self):
        values = resolver.read_dependencies(self.root)
        self.assertEqual(values["PSIMEDIA_SRTP_REF"], self.lock["psimedia"]["commit"])
        self.assertEqual(values["QTKEYCHAIN_COMMIT"], self.lock["qtkeychain"]["commit"])
        self.assertEqual(values["MSVC_QT6_SDK_TAG"], self.lock["windows_sdks"]["msvc_qt6"]["tag"])
        self.assertNotIn("QCA_TAG", values)
        self.assertEqual(values["IRIS_RELEASE_TAG"], self.lock["iris"]["release_tag"])

    def test_source_only_revision_exports_no_sdk_and_never_checks_a_tag(self):
        self.lock["iris"]["release_tag"] = None
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        values = resolver.read_dependencies(self.root)
        self.assertEqual(values["IRIS_RELEASE_TAG"], "")
        with patch.object(resolver.subprocess, "check_output", return_value=
                          "160000 commit " + "a" * 40 + "\tiris\n") as run:
            resolver.verify_iris_release(self.root, values["IRIS_RELEASE_TAG"])
            self.assertEqual(run.call_count, 1)
        with patch.object(resolver.subprocess, "check_output", return_value=""):
            with self.assertRaisesRegex(ValueError, "gitlink"):
                resolver.verify_iris_release(self.root, "")

    def test_psi_qca_pin_matches_iris_and_rejects_divergence(self):
        self.with_iris()
        values = resolver.read_dependencies(self.root, require_iris=True)
        self.assertEqual(values["QCA_TAG"], self.lock["qca"]["tag"])
        self.assertEqual(values["QCA_COMMIT"], self.lock["qca"]["commit"])
        self.assertEqual(values["QCA_MIN_VERSION"], "3.0.10")
        self.lock["qca"]["commit"] = "b" * 40
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        with self.assertRaisesRegex(ValueError, "differs from Iris"):
            resolver.read_dependencies(self.root, require_iris=True)

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

    def test_iris_release_requires_a_versioned_tag_without_a_second_sha(self):
        for tag in ("master", "v1.3/other", "../v1.3", "v1.3^{commit}", ""):
            with self.subTest(tag=tag):
                self.lock["iris"]["release_tag"] = tag
                (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
                with self.assertRaisesRegex(ValueError, "Invalid Iris release tag"):
                    resolver.read_dependencies(self.root)
        self.lock["iris"]["release_tag"] = "v1.3"
        self.lock["iris"]["commit"] = "a" * 40
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        with self.assertRaisesRegex(ValueError, "exact gitlink"):
            resolver.read_dependencies(self.root)

    def verify_release(self, remote, entry=None):
        entry = entry if entry is not None else "160000 commit " + "a" * 40 + "\tiris\n"
        tag = self.lock["iris"]["release_tag"]
        with patch.object(resolver.subprocess, "check_output", side_effect=[entry, remote]) as run:
            resolver.verify_iris_release(self.root, tag)
            self.assertEqual(run.call_args_list[1].args[0][-2:],
                             ["refs/tags/" + tag, "refs/tags/" + tag + "^{}"])

    def test_release_verification_accepts_lightweight_and_annotated_tags(self):
        ref = "refs/tags/" + self.lock["iris"]["release_tag"]
        self.verify_release("a" * 40 + "\t" + ref + "\n")
        # Annotated tag objects have their own SHA; compare the peeled commit.
        self.verify_release("b" * 40 + "\t" + ref + "\n" + "a" * 40 + "\t" + ref + "^{}\n")

    def test_release_verification_rejects_missing_or_different_source(self):
        ref = "refs/tags/" + self.lock["iris"]["release_tag"]
        with self.assertRaisesRegex(ValueError, "does not exist"):
            self.verify_release("")
        with self.assertRaisesRegex(ValueError, "does not match"):
            self.verify_release("b" * 40 + "\t" + ref + "\n")
        with self.assertRaisesRegex(ValueError, "does not match"):
            self.verify_release("a" * 40 + "\t" + ref + "\n" + "b" * 40 + "\t" + ref + "^{}\n")
        with self.assertRaisesRegex(ValueError, "gitlink"):
            self.verify_release("", entry="040000 tree " + "a" * 40 + "\tiris\n")

    def test_dependency_pins_are_not_duplicated_in_workflows(self):
        workflow_dir = ROOT / ".github/workflows"
        pins = [
            self.lock["iris"]["release_tag"],
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
