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
        (self.root / "dependencies.lock.json").write_text(json.dumps(self.lock))

    def with_iris(self, tag="v3.1.2", minimum="3.0.10"):
        iris_dir = self.root / "iris"
        iris_dir.mkdir()
        (iris_dir / "dependencies.lock.json").write_text(json.dumps({
            "qca": {
                "tag": tag,
                "commit": "c" * 40,
                "minimum_version": minimum,
            }
        }))

    def test_resolves_windows_and_psimedia_without_iris_checkout(self):
        values = resolver.read_dependencies(self.root)
        self.assertEqual(values["PSIMEDIA_SRTP_REF"], self.lock["psimedia"]["commit"])
        self.assertEqual(values["QTKEYCHAIN_COMMIT"], self.lock["qtkeychain"]["commit"])
        self.assertEqual(values["MSVC_QT6_SDK_TAG"], self.lock["windows_sdks"]["msvc_qt6"]["tag"])
        self.assertNotIn("QCA_TAG", values)
        self.assertNotIn("IRIS_RELEASE_TAG", values)
        self.assertEqual(values["PLUGINS_RELEASE_TAG"], self.lock["plugins"]["release_tag"])
        self.assertNotIn("PLUGINS_REF", values)

    def test_qca_comes_only_from_the_pinned_iris(self):
        self.assertNotIn("qca", self.lock)
        self.with_iris()
        values = resolver.read_dependencies(self.root, require_iris=True)
        self.assertEqual(values["QCA_TAG"], "v3.1.2")
        self.assertEqual(values["QCA_COMMIT"], "c" * 40)
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

    def test_plugins_use_a_release_tag_without_source_commit_pins(self):
        self.assertEqual(set(self.lock["plugins"]), {"release_tag"})
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
        self.assertIn("$arguments = @{", profile)
        self.assertIn("$arguments['BuildOnly'] = $true", profile)
        self.assertIn("$arguments['RunTests'] = $true", profile)
        self.assertIn("$arguments['ReleaseVersion'] = $env:RELEASE_VERSION", profile)
        self.assertIn("build_profile_msvc.ps1 @arguments", profile)
        self.assertNotIn("$arguments = @(", profile)
        self.assertIn("name: Upload all-in-one release packages\n        if: inputs.package", profile)
        self.assertIn("workflow_call:", packaging)
        self.assertIn("workflow_dispatch:", packaging)
        self.assertIn("package: true", packaging)
        self.assertIn("windows-msvc-qt6-profile.yml", packaging)
        self.assertIn("windows-msvc-qt6-packaging.yml", release)
        self.assertNotIn("windows-msvc-qt6-profile.yml", release)
        self.assertIn("if ($BuildOnly) {", builder)
        self.assertIn("Invoke-Checked cmake --install $buildDir", builder)

    def test_five_heavy_ci_platforms_no_automatic_installers(self):
        root = ROOT / ".github/workflows"
        ubuntu = (root / "ubuntu.yml").read_text()
        fedora = (root / "fedora.yml").read_text()
        mac = (root / "macos-arm64.yml").read_text()
        win5 = (root / "windows-msys2-qt5.yml").read_text()
        win6 = (root / "windows-msvc-qt6.yml").read_text()
        for workflow in (ubuntu, fedora, mac):
            self.assertIn("ccache", workflow)
            self.assertIn("USE_QT6=ON", workflow)
            self.assertNotIn("matrix:", workflow)
            self.assertNotIn("cmake --install build", workflow)
        for workflow in (ubuntu, mac):
            self.assertIn("CHAT_TYPE=BASIC", workflow)
        self.assertIn("CHAT_TYPE=WEBENGINE", fedora)
        self.assertIn("qt6-qtwebengine-devel", fedora)
        self.assertEqual(sum("CHAT_TYPE=WEBENGINE" in wf for wf in (ubuntu, fedora, mac)), 1)
        linux_selector = (ROOT / ".github/scripts/select-linux-iris.sh").read_text()
        for distro, workflow in (("ubuntu", ubuntu), ("fedora", fedora)):
            self.assertIn('BUNDLED_IRIS="$IRIS_BUNDLED"', workflow, distro)
            self.assertIn("select-linux-iris.sh", workflow, distro)
            self.assertIn("IRIS_BUNDLED_QCA=OFF", workflow, distro)
            self.assertIn("IRIS_SYSTEM_QCA=3", workflow, distro)
            self.assertNotIn("build-system-iris.sh", workflow, distro)
        self.assertIn("resolve-dependencies.py --resolve-iris-release", linux_selector)
        self.assertIn("iris_bundled=OFF", linux_selector)
        self.assertIn("iris_bundled=ON", linux_selector)
        self.assertIn("probe_installed_iris", linux_selector)
        self.assertNotIn("cmake --install", linux_selector)
        self.assertNotIn("git clone", linux_selector)
        self.assertNotIn("test ! -e iris/CMakeLists.txt", linux_selector)
        self.assertFalse((ROOT / ".github/scripts/build-system-iris.sh").exists())
        self.assertIn("runs-on: ubuntu-24.04", ubuntu)
        self.assertIn("container: fedora:latest", fedora)
        self.assertIn("runs-on: macos-15", mac)
        self.assertIn("BUNDLED_IRIS=ON", mac)
        self.assertIn("IRIS_BUNDLED_QCA=ON", mac)
        self.assertIn("modules: qtmultimedia qtimageformats", mac)
        self.assertNotIn("modules: qtmultimedia qtsvg", mac)
        self.assertIn("git submodule update --init --depth 1 iris", mac)
        self.assertNotIn("iris-qt6-macos-arm64.zip", mac)
        self.assertNotIn("BUNDLED_IRIS=OFF", mac)
        self.assertIn("chat_type: WEBKIT", win5)
        self.assertIn("default: WEBKIT", (root / "windows-msys2-qt5-profile.yml").read_text())
        mac_defs = (ROOT / "mac/macos_definitions.cmake").read_text()
        mac_src = (ROOT / "src/CMakeLists.txt").read_text()
        advwidget_cpp = (ROOT / "src/tools/advwidget/advwidget.cpp").read_text()
        advwidget_h = (ROOT / "src/tools/advwidget/advwidget.h").read_text()
        minizip_io = (ROOT / "src/tools/zip/minizip/ioapi.c").read_text()
        self.assertIn("screen->availableGeometry()", advwidget_cpp)
        self.assertNotIn("qApp->desktop()", advwidget_cpp)
        self.assertIn("Q_ENUM(Policy)", (ROOT / "src/widgets/pixmapratiolabel.h").read_text())
        self.assertIn("Q_ENUM(Shape)", (ROOT / "src/widgets/fancylabel.h").read_text())
        self.assertNotIn("Q_ENUMS(Qt::ToolButtonStyle)", (ROOT / "src/widgets/actionlineedit.h").read_text())
        self.assertNotIn("Q_OS_MAC\n", advwidget_cpp)
        self.assertNotIn("Q_OS_MAC\n", advwidget_h)
        self.assertIn("void fill_fopen_filefunc (zlib_filefunc_def* pzlib_filefunc_def)", minizip_io)
        self.assertNotIn("void fill_fopen_filefunc (pzlib_filefunc_def)", minizip_io)
        mac_builder = (ROOT / "mac/build-using-homebrew.sh").read_text()
        widget_ci = (root / "mam-native-widgets.yml").read_text()
        self.assertIn("cmake_minimum_required(VERSION 3.5)", mac_defs)
        self.assertNotIn("_qt5Core_install_prefix", mac_defs)
        self.assertIn("find_program(MACDEPLOYQT", mac_defs)
        self.assertNotIn("Qt5MacExtras", mac_src)
        self.assertIn("macOS requires Qt6", mac_src)
        self.assertIn("-DUSE_QT6=ON", mac_builder)
        self.assertNotIn("qt@5", mac_builder)
        self.assertNotIn("matrix:", widget_ci)
        self.assertNotIn("qtbase5-dev", widget_ci)
        self.assertIn("-DQT_DEFAULT_MAJOR_VERSION=6", widget_ci)
        self.assertFalse((root / "ai-windows-gcc-basic.yml").exists())
        qhttp_cmake = (ROOT / "3rdparty/qhttp.cmake").read_text()
        self.assertIn("add_library(qhttp", qhttp_cmake)
        self.assertIn("STATIC", qhttp_cmake)
        self.assertIn("target_compile_definitions(qhttp PUBLIC QHTTP_EXPORT)", qhttp_cmake)
        self.assertIn("package: false", win5)
        self.assertIn("run_tests: true", win5)
        self.assertIn("package: false", win6)
        self.assertIn("ccache", (root / "windows-msvc-qt6-profile.yml").read_text())
        self.assertIn("ccache", (root / "windows-msys2-qt5-profile.yml").read_text())
        for name in ("mam-native-integration.yml", "mam-prosody-runtime.yml",
                     "psimedia-integration.yml"):
            content = (root / name).read_text()
            self.assertIn("workflow_dispatch:", content)
            self.assertNotIn("pull_request:", content)
            self.assertNotIn("  push:", content)

    def test_iris_revision_and_release_come_only_from_the_gitlink(self):
        self.assertEqual(set(self.lock["iris"]), {"minimum_version"})
        for field in ("commit", "release_tag"):
            with self.subTest(field=field):
                lock = json.loads(json.dumps(self.lock))
                lock["iris"][field] = "v1.3"
                (self.root / "dependencies.lock.json").write_text(json.dumps(lock))
                with self.assertRaisesRegex(ValueError, "exact gitlink"):
                    resolver.read_dependencies(self.root)

    def resolve_release(self, remote, entry=None):
        entry = entry if entry is not None else "160000 commit " + "a" * 40 + "\tiris\n"
        with patch.object(resolver.subprocess, "check_output",
                          side_effect=[entry, remote]) as run:
            result = resolver.resolve_iris_release(self.root)
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args_list[1].args[0],
                             ["git", "ls-remote", "--tags", "https://github.com/psi-im/iris.git"])
            return result

    def test_gitlink_with_a_lightweight_or_annotated_release_tag_uses_it(self):
        self.assertEqual(self.resolve_release("a" * 40 + "\trefs/tags/v1.5.1\n"), "v1.5.1")
        self.assertEqual(self.resolve_release("b" * 40 + "\trefs/tags/v1.5.1\n"
                                              + "a" * 40 + "\trefs/tags/v1.5.1^{}\n"), "v1.5.1")

    def test_gitlink_with_several_release_tags_uses_the_newest(self):
        self.assertEqual(self.resolve_release("a" * 40 + "\trefs/tags/v1.5\n"
                                              + "a" * 40 + "\trefs/tags/v1.10.0\n"
                                              + "a" * 40 + "\trefs/tags/v1.9.2\n"), "v1.10.0")
        # Other tags at the same commit are not releases, and do not get in the way.
        self.assertEqual(self.resolve_release("a" * 40 + "\trefs/tags/nightly\n"
                                              + "a" * 40 + "\trefs/tags/v1.5.1\n"), "v1.5.1")

    def test_gitlink_without_a_release_tag_builds_from_source(self):
        self.assertEqual(self.resolve_release(""), "")
        # Another commit's release must never stand in for this one...
        self.assertEqual(self.resolve_release("b" * 40 + "\trefs/tags/v1.5.1\n"), "")
        self.assertEqual(self.resolve_release("b" * 40 + "\trefs/tags/v1.5.1\n"
                                              + "c" * 40 + "\trefs/tags/v1.5.1^{}\n"), "")
        # ...nor does a tag at this commit make it a release.
        self.assertEqual(self.resolve_release("a" * 40 + "\trefs/tags/nightly\n"), "")

    def test_failed_lookups_build_from_source(self):
        with patch.object(resolver.subprocess, "check_output", side_effect=OSError("offline")):
            self.assertEqual(resolver.resolve_iris_release(self.root), "")
        with patch.object(resolver.subprocess, "check_output", return_value="invalid"):
            self.assertEqual(resolver.resolve_iris_release(self.root), "")
        with patch.object(resolver.subprocess, "check_output",
                          return_value="040000 tree " + "a" * 40 + "\tiris\n"):
            self.assertEqual(resolver.resolve_iris_release(self.root), "")
        self.assertEqual(self.resolve_release("not a tag line\n"), "")

    def test_cli_looks_up_the_iris_release_only_on_request(self):
        import io
        from contextlib import redirect_stdout
        for arguments, expected in (([], False), (["--resolve-iris-release"], True)):
            with self.subTest(arguments=arguments), \
                 patch.object(resolver, "read_dependencies", return_value={}), \
                 patch.object(resolver, "resolve_iris_release", return_value="v1.5.1") as resolve, \
                 patch.dict(resolver.os.environ, {}, clear=True), \
                 patch.object(resolver.sys, "argv", ["resolve-dependencies.py"] + arguments), \
                 redirect_stdout(io.StringIO()) as output:
                resolver.main()
                self.assertEqual(resolve.called, expected)
                self.assertEqual("IRIS_RELEASE_TAG=v1.5.1\n" in output.getvalue(), expected)

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
        }
        for path, needle in workflows.items():
            self.assertIn(needle, (ROOT / ".github/workflows" / path).read_text(), path)


if __name__ == "__main__":
    unittest.main()
