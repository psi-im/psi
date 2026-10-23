# Dependency policy for Psi

Psi uses `dependencies.lock.json` for dependencies managed by the Psi CI. Do **not** copy QCA's pinned release into this file: Iris is an exact Git submodule, and its own `iris/dependencies.lock.json` remains the authoritative QCA tag, Git commit and minimum compatible version.

## Lock contents

- `psimedia.commit`: exact provider revision for the Jingle/media integration tests (changing the provider API requires updating the tests and this SHA).
- `qtkeychain.tag` / `qtkeychain.commit`: the source tag and the resolved Git SHA for Windows Qt5/Qt6 dependency SDK builds. Clones check that the tag resolves to the locked commit.
- `plugins.release_tag`: the sole selector for official plugin binaries in an explicitly versioned Psi **all-in-one** release. Neither a Plugins source commit nor a Psi Plugin API source commit is pinned in Psi's lock: release tags may move. The released archive's Qt target, platform, QCA **tag**, and each DLL's SHA-256 hash are checked during staging. Ordinary Windows branch/PR CI only compiles and runs regression tests: no installation staging, NSIS, ZIP, or released plugins/PsiMedia. Manual all-in-one packaging and Psi release publishing use the same reusable `.github/workflows/windows-msvc-qt6-packaging.yml` entry point, which performs the installer smoke test before producing artifacts.
- `windows_sdks.msvc_qt6` and `windows_sdks.msys2_qt5`: release tags and ZIP archive names for Psi's platform-specific SDKs. These releases are artifacts produced by Psi; changing their contents requires changing the release tag in the lock to avoid reusing incompatible binaries.
- `iris`: describes the submodule, its lock path and `release_tag`, the coordinate of the matching published SDK. Its source revision remains exclusively in the `iris` gitlink, not a duplicated SHA field. Update the gitlink and release tag together when moving to another release.
  An unreleased development gitlink can use `release_tag: null`. Even if the lock
  still contains an older tag, the verified SDK coordinate is empty whenever
  the pinned gitlink is not released under that tag. CI then builds the exact
  Iris source; it must never substitute the previous release's binaries.

## Consumption

The shared `.github/actions/load-dependencies` action validates and exports the lock. It publishes `iris_release_tag`, `psimedia_ref`, `qtkeychain_tag`, `qtkeychain_commit`, `msvc_sdk_tag`, `msvc_sdk_archive`, `msys2_sdk_tag`, `msys2_sdk_archive` as step outputs, and their uppercase versions in the job environment.

`resolve-dependencies.py --verify-iris-release` resolves the effective Iris SDK tag
against the exact gitlink, including peeled annotated tags. On Psi branch and PR
builds, a stale or unavailable Iris release falls back to the bundled source,
without failing CI. A tagged Psi build also accepts an unreleased Iris gitlink
and builds it from source, but rejects a gitlink released under a different tag
than the lock declares. Failed remote verification on tagged Psi builds is fatal
rather than risking an unverified SDK. Normal lock resolution remains offline.

Linux full-client CI compiles the exact Iris gitlink directly within Psi's CMake
build (BUNDLED_IRIS=ON), rather than separately building/installing a system
Iris SDK. Ubuntu installs locked shared QCA3/Qt6 from Iris's release assets;
Fedora builds shared QCA3/Qt6 from Iris's locked QCA source revision. Iris
links to that QCA3 runtime rather than bundling another copy. Releases of Iris
remain relevant to workflows that intentionally consume external Iris SDKs,
but are not substituted for the gitlink in ordinary full-client Linux CI.

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
