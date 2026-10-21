# Psi direct dependency lock and Qt release contract

`dependencies.lock.json` is the single source for **direct** Psi build,
packaging and plugin dependencies. Release profiles target **Qt 6.12.0** for
Windows, macOS and Android SDK consumers. Windows legacy Qt5 remains isolated.

## Locked coordinates

- `qt.qt6.version`: exact Qt 6.12.0 release baseline (not a floating 6.12.* series).
- `qt.qt5_windows_legacy`: official Qt5.15.2 MSVC/Win7 media baseline;
  MSYS2 Qt5 uses its independently updated distribution packages.
- `iris.release_tag` and the Git `iris` submodule: source and released SDK
  must resolve to the **same commit**.
- `qca.repository/release_tag/commit/minimum_version`: Psi directly links
  against a shared QCA SDK on Windows and loads its crypto provider. This
  duplicate *consumer coordinate* must agree **exactly** with QCA pinned
  inside the checked-out Iris `dependencies.lock.json`; the Iris lock
  remains authoritative for the upstream QCA dependency relationship.
- `psimedia` and `plugins`: exact commit and nullable release tag.
  A null tag means source validation only: no unverified binary may be
  substituted. PsiMedia must use the same Psi media/plugin API and Qt release.
- `qtkeychain.tag/commit`: exact installed SDK revision.
- `source_submodules`: exact source revision for qhttp, http-parser and qite,
  which must match their respective Git submodule entries.
- `translations.commit`: exact Psi translation snapshot; Windows packaging
  uses a detached checkout of this commit, **not** the moving l10n master.
- `media_runtime`: required GStreamer versions for supported Windows profiles.
- `windows_sdks`: immutable release tag and archive for modern Qt6 and
  legacy Qt5 dependency SDKs. **Bump the SDK release tag** whenever any
  bundled QCA, QtKeychain or compiled SDK dependency changes.

## Release dependency sequence

1. Build and test QCA with Qt 6.12.0 across Windows, macOS and Android,
   then tag and publish a **new** QCA release with SDK artifacts. Do not
   overwrite the old tag's binary assets with different Qt builds.
2. Update Iris's QCA lock, run Qt5/Qt6 tests against the new QCA, tag and
   release Iris; update Psi's Iris gitlink, Iris tag and direct QCA pin
   atomically.
3. Build PsiMedia and ordinary plugins separately, pinning the Psi plugin
   API SHA. Publish their independently verified Windows/macOS binaries and
   record exact commit/tag in Psi's lock.
4. Build a new Psi Windows dependency SDK tagged for Qt6.12, then assemble
   the Psi application, ordinary plugins, PsiMedia and GStreamer runtime
   from immutable binaries. Verify expected plugin DLLs, ABI, runtime
   closure and source/version manifests before producing NSIS and ZIP.
5. Release Psi using exact source/tag revisions. Never report a complete
   alpha while upstream component archives or runtime checks are missing.

`.github/actions/load-dependencies` exports validated source/tag coordinates.
`resolve-dependencies.py --require-iris` additionally asserts that the
direct QCA pin matches Iris. `--verify-iris-release` checks the tagged
Iris commit against the `iris` Git submodule, including annotated tags.
`tests/dependency_lock/test_lock.py` exercises these invariants.

This lock records exact coordinates but **does not** replace compilation or
runtime ABI checks. The Qt5 distro CI and standalone macOS/Android app
distribution paths have separate platform constraints.
