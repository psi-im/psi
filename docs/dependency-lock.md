# Dependency policy for Psi

Psi uses `dependencies.lock.json` for dependencies managed by the Psi CI. Iris is an exact Git submodule, and its own `iris/dependencies.lock.json` remains the authoritative QCA tag, Git commit and minimum compatible version, and the authority for JDNS. Psi repeats only the QCA tag and commit, for release packaging; `resolve-dependencies.py` rejects a copy that differs from the lock of the pinned Iris, so move the gitlink and this copy together.

Source gitlinks (`iris`, `3rdparty/*`) pin themselves in Git and are not listed in the lock.

## Lock contents

- `psimedia.commit`: exact provider revision for the Jingle/media integration tests (changing the provider API requires updating the tests and this SHA).
- `qtkeychain.tag` / `qtkeychain.commit`: the source tag and the resolved Git SHA for Windows Qt5/Qt6 dependency SDK builds. Clones check that the tag resolves to the locked commit.
- `plugins.release_tag`: the sole selector for official plugin binaries in an explicitly versioned Psi **all-in-one** release. Neither a Plugins source commit nor a Psi Plugin API source commit is pinned in Psi's lock: release tags may move. The released archive's Qt target, platform, QCA **tag**, and each DLL's SHA-256 hash are checked during staging. Ordinary Windows branch/PR CI only compiles and runs regression tests: no installation staging, NSIS, ZIP, or released plugins/PsiMedia. Manual all-in-one packaging and Psi release publishing use the same reusable `.github/workflows/windows-msvc-qt6-packaging.yml` entry point, which performs the installer smoke test before producing artifacts.
- `windows_sdks.msvc_qt6` and `windows_sdks.msys2_qt5`: release tags and ZIP archive names for Psi's platform-specific SDKs. These releases are artifacts produced by Psi; changing their contents requires changing the release tag in the lock to avoid reusing incompatible binaries.
- `iris.minimum_version`: the API floor for builds against an installed Iris, read by `CMakeLists.txt`. The source revision is the `iris` gitlink, and its release is whatever tag that gitlink has: neither is lock data.

## Consumption

The shared `.github/actions/load-dependencies` action validates and exports the lock. It publishes `psimedia_ref`, `qtkeychain_tag`, `qtkeychain_commit`, `msvc_sdk_tag`, `msvc_sdk_archive`, `msys2_sdk_tag`, `msys2_sdk_archive` as step outputs, and their uppercase versions in the job environment.

Linux full-client CI selects the Iris provider from the exact gitlink, which
`resolve-dependencies.py --resolve-iris-release` looks up in the Iris tags,
including peeled annotated ones. When that gitlink is a release and a compatible
SDK for the Linux distribution and Qt major is published, Psi uses that SDK with
BUNDLED_IRIS=OFF. Otherwise, including unreleased commits, a failed lookup, or
missing/incompatible distro packages, Psi compiles the initialized Iris git
submodule in-tree with BUNDLED_IRIS=ON. It never substitutes another release's
binaries, and we never build and install another Iris from the same source in a
separate CI step. Ubuntu uses the locked shared QCA3/Qt6 package; Fedora builds
QCA3/Qt6 from Iris's locked QCA source. Fedora currently has no published Iris
RPM release asset, so its normal path remains bundled until a compatible SDK
distribution is available.

JDNS follows from Iris: `iris/dependencies.lock.json` pins it, and Iris uses a
compatible system QJDns or builds that pinned commit along with itself.

Use the action **after** checkout. If the workflow needs QCA, initialize Iris first, then specify `require-iris: 'true'`. To install a QCA release package, pass `qca-install-mode: apt` and the platform-specific `qca-asset` (for Ubuntu 24.04 this is `qca3-deb-ubuntu-24.04.zip`); the Psi action delegates the installation to Iris's shared action. The version remains defined only by Iris.

For reusable Windows workflows, the resolver job reads the Psi lock and passes its step outputs as reusable workflow inputs; the callee also validates that any explicitly supplied SDK coordinates match the lock. The SDK builders validate QtKeychain's commit, and the MSYS2 Qt5 builder also verifies QCA's Git commit.

Do not write `QCA_TAG: v...`, QtKeychain version, PsiMedia SHA or versioned SDK tags directly into individual YAML workflows. A new version is changed once in the appropriate lock and propagated to all consumers. `tests/dependency_lock/test_lock.py` and the `Dependency lock verification` workflow guard the contract.

Ordinary full-client CI has exactly five compile/test targets: Windows 7 Qt5,
Windows MSVC Qt6, Ubuntu 24.04 Qt6, macOS arm64 Qt6 and Fedora latest Qt6.
All use compiler caching. Fedora exercises Qt6/WebEngine; Windows 7 retains
the legacy Qt5/WebKit coverage from its original CI; the remaining three
full-client targets use BASIC. The Linux/macOS profiles do not build official plugins.
macOS is Qt6-only and always builds Iris from Psi's exact gitlink with bundled
QCA/UsrSCTP/OMEMO; it does not consume released Iris SDK binaries. Qt5 remains
only in the Windows 7 (MSYS2) compatibility profile; Linux, Windows MSVC,
macOS, and focused widget tests use Qt6.
Expensive protocol integration suites remain manually dispatchable;
small path-filtered widget and component regressions can run automatically.

System distribution packages, toolchain installation, and Qt installer package series are intentionally not treated as exact source locks. If we need reproducible toolchain binaries, they require separate pinned package indexes and hashes rather than merely moving a variable into this JSON file.
