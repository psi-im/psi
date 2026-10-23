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
        self.assertEqual(values["PLUGINS_RELEASE_TAG"], self.lock["plugins"]["release_tag"])
        self.assertNotIn("PLUGINS_REF", values)

    def test_source_only_revision_exports_no_sdk_and_never_checks_a_tag(self):
        self.lock["iris"]["release_tag"] = None
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        values = resolver.read_dependencies(self.root)
        self.assertEqual(values["IRIS_RELEASE_TAG"], "")
        with patch.object(resolver.subprocess, "check_output") as run:
            self.assertEqual(resolver.resolve_iris_release(self.root, "", strict=True), "")
            run.assert_not_called()

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

    def test_plugins_use_a_release_tag_without_source_commit_pins(self):
        self.assertEqual(set(self.lock["plugins"]), {"repository", "release_tag"})
        for tag in ("", None, "../1.6", "1.6^{commit}"):
            with self.subTest(tag=tag):
                self.lock["plugins"]["release_tag"] = tag
                (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
                with self.assertRaisesRegex(ValueError, "Invalid official plugin release tag"):
                    resolver.read_dependencies(self.root)
        self.lock["plugins"]["release_tag"] = "1.6"
        self.lock["plugins"]["commit"] = "a" * 40
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))
        with self.assertRaisesRegex(ValueError, "Invalid official plugin release tag"):
            resolver.read_dependencies(self.root)

    def test_release_only_stages_external_extensions(self):
        profile = (ROOT / ".github/workflows/windows-msvc-qt6-profile.yml").read_text()
        builder = (ROOT / "packaging/windows/build_profile_msvc.ps1").read_text()
        self.assertIn("if: inputs.package", profile)
        self.assertIn("if ($ReleaseVersion) {\n    Invoke-Checked python packaging/windows/stage_released_extensions.py", builder)

    def test_windows_ci_and_release_share_explicit_packaging_entry_point(self):
        workflow_root = ROOT / ".github/workflows"
        ci = (workflow_root / "windows-msvc-qt6.yml").read_text()
        profile = (workflow_root / "windows-msvc-qt6-profile.yml").read_text()
        packaging = (workflow_root / "windows-msvc-qt6-packaging.yml").read_text()
        release = (workflow_root / "psi-1.6-beta1-release.yml").read_text()
        builder = (ROOT / "packaging/windows/build_profile_msvc.ps1").read_text()
        self.assertIn("package: false", ci)
        self.assertIn("if: inputs.package", profile)
        self.assertIn("'-BuildOnly', '-RunTests'", profile)
        self.assertIn("name: Upload all-in-one release packages\n        if: inputs.package", profile)
        self.assertIn("workflow_call:", packaging)
        self.assertIn("workflow_dispatch:", packaging)
        self.assertIn("package: true", packaging)
        self.assertIn("windows-msvc-qt6-profile.yml", packaging)
        self.assertIn("windows-msvc-qt6-packaging.yml", release)
        self.assertNotIn("windows-msvc-qt6-profile.yml", release)
        self.assertIn("if ($BuildOnly) {", builder)
        self.assertIn("Invoke-Checked cmake --install $buildDir", builder)

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

    def resolve_release(self, remote, entry=None, strict=False):
        entry = entry if entry is not None else "160000 commit " + "a" * 40 + "\tiris\n"
        tag = self.lock["iris"]["release_tag"]
        with patch.object(resolver.subprocess, "check_output",
                          side_effect=[entry, remote]) as run:
            result = resolver.resolve_iris_release(self.root, tag, strict=strict)
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args_list[1].args[0],
                             ["git", "ls-remote", "--tags", "https://github.com/psi-im/iris.git"])
            return result

    def test_matching_lightweight_and_annotated_tags_use_sdk(self):
        ref = "refs/tags/" + self.lock["iris"]["release_tag"]
        self.assertEqual(self.resolve_release("a" * 40 + "\t" + ref + "\n"), "v1.5.1")
        self.assertEqual(self.resolve_release("b" * 40 + "\t" + ref + "\n"
                                              + "a" * 40 + "\t" + ref + "^{}\n",
                                              strict=True), "v1.5.1")

    def test_branch_with_stale_iris_tag_uses_source_instead_of_old_sdk(self):
        ref = "refs/tags/v1.5.1"
        self.assertEqual(self.resolve_release("b" * 40 + "\t" + ref + "\n"), "")
        self.assertEqual(self.resolve_release(""), "")
        self.assertEqual(self.resolve_release(
            "b" * 40 + "\t" + ref + "\n"
            + "a" * 40 + "\trefs/tags/v1.5.2\n"), "")

    def test_tagged_psi_with_untagged_iris_uses_source(self):
        # A release of Psi can include a newer, unreleased Iris gitlink.
        self.assertEqual(self.resolve_release(
            "b" * 40 + "\trefs/tags/v1.5.1\n", strict=True), "")
        self.assertEqual(self.resolve_release("", strict=True), "")

    def test_tagged_psi_rejects_a_different_iris_release(self):
        remote = ("b" * 40 + "\trefs/tags/v1.5.1\n"
                  + "a" * 40 + "\trefs/tags/v1.5.2\n")
        with self.assertRaisesRegex(ValueError, "tagged v1.5.2"):
            self.resolve_release(remote, strict=True)
        self.assertEqual(self.resolve_release(remote, strict=False), "")

    def test_remote_failures_are_only_fatal_on_tagged_psi(self):
        with patch.object(resolver.subprocess, "check_output",
                          side_effect=OSError("offline")):
            self.assertEqual(resolver.resolve_iris_release(self.root, "v1.5.1"), "")
            with self.assertRaises(OSError):
                resolver.resolve_iris_release(self.root, "v1.5.1", strict=True)
        with self.assertRaisesRegex(ValueError, "gitlink"):
            self.resolve_release("", entry="040000 tree " + "a" * 40 + "\tiris\n",
                                 strict=True)
        # Ordinary development builds are not failed by the release-only gate.
        with patch.object(resolver.subprocess, "check_output",
                          return_value="invalid"):
            self.assertEqual(resolver.resolve_iris_release(self.root, "v1.5.1"), "")

    def test_cli_marks_only_psi_tagged_builds_strict(self):
        import io
        from contextlib import redirect_stdout
        for ref, expected in (("refs/heads/master", False),
                              ("refs/pull/1032/merge", False),
                              ("refs/tags/1.6_b2", True)):
            with self.subTest(ref=ref), \
                 patch.object(resolver, "read_dependencies",
                              return_value={"IRIS_RELEASE_TAG": "v1.5.1"}), \
                 patch.object(resolver, "resolve_iris_release",
                              return_value="") as resolve, \
                 patch.dict(resolver.os.environ, {"GITHUB_REF": ref}, clear=True), \
                 patch.object(resolver.sys, "argv",
                              ["resolve-dependencies.py", "--verify-iris-release"]), \
                 redirect_stdout(io.StringIO()) as output:
                resolver.main()
                self.assertIn("IRIS_RELEASE_TAG=\n", output.getvalue())
                self.assertEqual(resolve.call_args.kwargs["strict"], expected)

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
