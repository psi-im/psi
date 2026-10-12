# Dependency policy for Psi

Psi uses `dependencies.lock.json` for dependencies managed by the Psi CI. Do **not** copy QCA's pinned release into this file: Iris is an exact Git submodule, and its own `iris/dependencies.lock.json` remains the authoritative QCA tag, Git commit and minimum compatible version.

## Lock contents

- `psimedia.commit`: exact provider revision for the Jingle/media integration tests (changing the provider API requires updating the tests and this SHA).
- `qtkeychain.tag` / `qtkeychain.commit`: the source tag and the resolved Git SHA for Windows Qt5/Qt6 dependency SDK builds. Clones check that the tag resolves to the locked commit.
- `windows_sdks.msvc_qt6` and `windows_sdks.msys2_qt5`: release tags and ZIP archive names for Psi's platform-specific SDKs. These releases are artifacts produced by Psi; changing their contents requires changing the release tag in the lock to avoid reusing incompatible binaries.
- `iris`: describes the submodule and the lock path, but its revision itself is maintained exclusively by the `iris` gitlink. Updating Iris requires an ordinary gitlink commit, not a duplicated SHA field.

## Consumption

The shared `.github/actions/load-dependencies` action validates and exports the lock. It publishes `psimedia_ref`, `qtkeychain_tag`, `qtkeychain_commit`, `msvc_sdk_tag`, `msvc_sdk_archive`, `msys2_sdk_tag`, `msys2_sdk_archive` as step outputs, and their uppercase versions in the job environment.

Use the action **after** checkout. If the workflow needs QCA, initialize Iris first, then specify `require-iris: 'true'`. To install a QCA release package, pass `qca-install-mode: apt` and the platform-specific `qca-asset` (for Ubuntu 24.04 this is `qca3-deb-ubuntu-24.04.zip`); the Psi action delegates the installation to Iris's shared action. The version remains defined only by Iris.

For reusable Windows workflows, the resolver job reads the Psi lock and passes its step outputs as reusable workflow inputs; the callee also validates that any explicitly supplied SDK coordinates match the lock. The SDK builders validate QtKeychain's commit, and the MSYS2 Qt5 builder also verifies QCA's Git commit.

Do not write `QCA_TAG: v...`, QtKeychain version, PsiMedia SHA or versioned SDK tags directly into individual YAML workflows. A new version is changed once in the appropriate lock and propagated to all consumers. `tests/dependency_lock/test_lock.py` and the `Dependency lock verification` workflow guard the contract.

System distribution packages (such as Fedora Qt5/QCA2), toolchain installation, and Qt installer package series are intentionally not treated as exact source locks. If we need reproducible toolchain binaries, they require separate pinned package indexes and hashes rather than merely moving a variable into this JSON file.
