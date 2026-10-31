"""Windows packages select native QCA providers without retaining stale DLLs."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[2] / "packaging/windows"
sys.path.insert(0, str(SCRIPTS))
from qca_providers import select_qca_providers, stage_qca_providers


def load_script(name):
    spec = importlib.util.spec_from_file_location("psi_" + name, SCRIPTS / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PACKAGERS = [load_script("prepare_package"), load_script("prepare_package_msvc")]


class QcaProviderStagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.sdk = self.directory / "sdk"
        self.stage = self.directory / "package"
        self.stage.mkdir()
        (self.sdk / "bin").mkdir(parents=True)
        (self.sdk / "bin/qca3-qt6.dll").write_bytes(b"core")
        self.plugins = self.sdk / "lib/qca3-qt6/crypto"
        self.plugins.mkdir(parents=True)

    def add(self, name, data=b"provider"):
        path = self.plugins / name
        path.write_bytes(data)
        return path

    def test_packagers_prefer_native_and_stage_both_sasl_providers(self):
        for name in ("qca-wincrypto.dll", "qca-cyrus-sasl.dll", "qca-wingss.dll", "qca-ossl.dll"):
            self.add(name, name.encode())
        for packager in PACKAGERS:
            with self.subTest(packager=packager.__name__):
                for folder in (self.stage / "crypto", self.stage / "qtplugins/crypto"):
                    folder.mkdir(parents=True, exist_ok=True)
                    (folder / "libQCA3-OSSLd.DLL").write_bytes(b"stale")
                (self.stage / "qtplugins/crypto/other.dll").write_bytes(b"unrelated")
                packager.copy_sdk_runtime(self.sdk, self.stage)
                for name in ("qca-wincrypto.dll", "qca-cyrus-sasl.dll", "qca-wingss.dll"):
                    self.assertEqual((self.stage / "qtplugins/crypto" / name).read_bytes(), name.encode())
                for folder in (self.stage / "crypto", self.stage / "qtplugins/crypto"):
                    self.assertFalse(any("ossl" in p.name.lower() for p in folder.iterdir()))
                self.assertEqual((self.stage / "qtplugins/crypto/other.dll").read_bytes(), b"unrelated")
                self.assertEqual((self.stage / "qca3-qt6.dll").read_bytes(), b"core")
        self.assertTrue((self.plugins / "qca-ossl.dll").exists())

    def test_native_sdk_does_not_need_an_openssl_provider(self):
        for name in ("qca-wincrypto.dll", "qca-cyrus-sasl.dll", "qca-wingss.dll"):
            self.add(name)
        for packager in PACKAGERS:
            packager.copy_sdk_runtime(self.sdk, self.stage)
        self.assertEqual(len(list((self.stage / "qtplugins/crypto").glob("*.dll"))), 3)

    def test_legacy_fallback_without_native_provider(self):
        self.add("libqca-ossl.dll", b"legacy")
        (self.stage / "crypto").mkdir()
        (self.stage / "crypto/qca-wincrypto.dll").write_bytes(b"old native")
        PACKAGERS[0].copy_sdk_runtime(self.sdk, self.stage)
        self.assertEqual((self.stage / "qtplugins/crypto/libqca-ossl.dll").read_bytes(), b"legacy")
        self.assertFalse((self.stage / "crypto/qca-wincrypto.dll").exists())

    def test_qt5_native_sdk_can_contain_only_sspi_for_sasl(self):
        self.add("libqca-wincrypto.dll")
        self.add("libqca-wingss.dll")
        PACKAGERS[0].copy_sdk_runtime(self.sdk, self.stage)
        self.assertTrue((self.stage / "qtplugins/crypto/libqca-wingss.dll").is_file())

    def test_msvc_requires_all_promised_providers_before_mutating_package(self):
        self.add("qca-wincrypto.dll")
        (self.stage / "crypto").mkdir()
        old = self.stage / "crypto/qca-ossl.dll"
        old.write_bytes(b"old")
        with self.assertRaisesRegex(SystemExit, "cyrus-sasl.*wingss"):
            PACKAGERS[1].copy_sdk_runtime(self.sdk, self.stage)
        self.assertEqual(old.read_bytes(), b"old")
        self.assertFalse((self.stage / "qca3-qt6.dll").exists())

    def test_missing_crypto_provider_is_rejected(self):
        self.add("qca-wingss.dll")
        with self.assertRaisesRegex(SystemExit, "neither"):
            select_qca_providers(self.sdk)

    def test_duplicate_provider_is_rejected(self):
        self.add("qca-wincrypto.dll")
        duplicate = self.sdk / "different-architecture/qca-wincrypto.dll"
        duplicate.parent.mkdir()
        duplicate.write_bytes(b"wrong architecture")
        with self.assertRaisesRegex(SystemExit, "Ambiguous"):
            stage_qca_providers(self.sdk, self.stage)
        self.assertFalse((self.stage / "qtplugins").exists())

    def test_provider_names_are_case_insensitive(self):
        provider = self.add("LIBQCA3-WINCRYPTO.DLL")
        self.assertEqual(select_qca_providers(self.sdk), [provider])

    def test_overlapping_source_and_package_are_rejected(self):
        self.add("qca-wincrypto.dll")
        for root in (self.sdk, self.sdk / "package", self.directory):
            with self.subTest(root=root), self.assertRaisesRegex(SystemExit, "overlap"):
                stage_qca_providers(self.sdk, root)
        self.assertTrue((self.plugins / "qca-wincrypto.dll").is_file())


if __name__ == "__main__":
    unittest.main()
