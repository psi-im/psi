# Qt 6.12 release pipeline audit (2026-10-21)

Status: **work in progress — do not tag or publish the Psi alpha until the dependent packages pass CI**.

## Supported matrix and gaps

| Repository | Windows modern | macOS | Android | Qt5 |
|---|---|---|---|---|
| QCA3 | Qt 6.12.0 MSVC, shared QCA SDK | Qt 6.12.0 SDK arm64/x86_64 | Qt 6.12.0 ABI SDK matrix | Separate Windows MSYS2 Qt5/Win7 build |
| Iris | Qt 6.12.0, consumes released QCA SDK | Qt 6.12.0 | Qt 6.12.0, consumes released QCA SDK | Qt5 Linux compatibility matrix maintained |
| PsiMedia | Qt 6.12.0 MSVC, standalone installed media plugin + bundled GStreamer | Qt 6.12.0 package workflow | **No Android package workflow exists** | Qt 5.15.2/MSVC2019 legacy Win7 profile; do not mix with modern Windows binaries |
| Official plugins | New standalone Windows Qt 6.12.0 and macOS Qt 6.12.0 artifact workflow | Same workflow | **No Android plugin package workflow exists** | Linux Qt5 tests plus separate Qt5 OTR/Win7 build |
| Psi | Qt 6.12.0 MSVC ZIP + NSIS; dependency SDK v4 | **No automated macOS Psi distribution workflow** | **No Android Psi distribution workflow** | Existing MSYS2 Qt5/Win7 best-effort profile |

Qt 6.12.0 was released 2026-09-30. Qt 5.15.19 exists upstream but the available Qt5 SDK differs by platform: in particular the Win7 MSVC2019 redistribution uses Qt 5.15.2. Retain existing Qt5 package toolchains until a newer compatible binary SDK is actually available; no implicit downgrade of Qt6 releases.

Qt 6.12 Android continues to support NDK r27c and minimum API 28. No reason to change those existing QCA/Iris settings.

## Release dependency graph

1. **QCA3**: merge validated Qt 6.12 CI/packaging changes, then publish **v3.0.12** with Windows/macOS/Android SDKs. Inspect the Qt version of every SDK; no binaries left at 6.10.
2. **Iris**: update `dependencies.lock.json` to exact QCA v3.0.12 tag/commit. Build/validate all Qt 6.12 platforms and Qt5 compatibility, then publish **v1.5.2** at the verified source commit (not simply the latest moving HEAD).
3. **PsiMedia**: build Windows and macOS packages against Qt 6.12 and an explicit compatible Psi plugin API commit. Windows packages must contain mediaplugin and a GStreamer runtime manifest. After green CI, create the next version tag (v1.3 if appropriate).
4. **Official plugins**: build binary artifacts against pinned Psi plugin API and QCA SDK with Qt 6.12. Include `PLUGINS-MANIFEST.json` with source SHA and hashes. Publish a versioned artifact/release after testing; do not infer a release from a short-lived workflow artifact without verifying its SHA and metadata.
5. **Psi**: bump shared Windows dependency SDK tag to `windows-sdk-msvc-qt6-v4`; update Iris gitlink to the released v1.5.2 source; replace PsiMedia and plugin source hashes/tags in `dependencies.lock.json` and validate QCA direct pin equals Iris QCA lock. Build the client with plugin *loading* enabled, **without recompiling PsiMedia or plugins**; download prebuilt components, verify manifests and runtime DLL conflicts, then assemble a single stage tree for ZIP and NSIS.
6. Publish `2.0-alpha1` **only** after full Windows x64 ZIP/installer verification, smoke launch, plugin enumeration, real media availability and checksum verification.

Versions in this section beyond already published tags are **proposed release tag names**, not existing artifacts.

## Direct dependency contract for Psi

`dependencies.lock.json` must describe the exact source revision for Iris (via gitlink plus matching release tag), QCA3, PsiMedia, official plugins, QtKeychain, translations, and all local source gitlinks; it must carry the Qt 6.12 platform target and the correct binary Windows SDK tag. The QCA pin may be redundant with Iris's lock but must be cross-checked automatically rather than allowed to diverge. A source checkout may exist before a release exists; do not label it released prematurely.

## Review of KDE QCA upstream

At audit time `psi-im/qca master` is **304 commits ahead** of the merge base with `KDE/qca master`, and **four commits behind** (`b158c98b...` merge base). Those four changes are:

- KDE `36913f2...`: upstream QCA **2.3.11 → 2.3.12** version bump; not applicable verbatim to our QCA3 numbering.
- KDE `c7de7c9...`: updated `rootcerts.pem` from August 2026; **already present in our fork** (identical Git blob SHA `0c1339ec...` and matching `certs/README` date).
- KDE `66026b6...`: LibreSSL compatibility changes in OpenSSL provider; the fork **already has the RSA LibreSSL guard**, wraps `openssl/kdf.h`, and independently guards Whirlpool on OpenSSL compile flags. The upstream-only `SSL_CIPHER_standard_name` alias is not needed in the fork, which no longer calls that API. No blind cherry-pick.
- KDE `e16fbbe...`: clang-format 19 → 21 plus formatting; no runtime fix.

**No upstream merge is currently necessary for the Qt 6.12 release.** The substantive changes have already been incorporated or addressed differently in QCA3. The remaining changes are upstream 2.x versioning and formatting. Keep crypto regressions in the CI matrix.

## Hard release gates

- QCA Qt 6.12 SDKs built and installed on Windows/macOS/Android.
- Iris Qt 6.12 builds use the *same* released QCA API and Qt version.
- PsiMedia and official plugins are obtained from their own verified builds, never recompiled in the Psi packaging job.
- The Psi EXE uses the same Qt 6.12 libraries as packaged plugins. A different Qt minor or an outdated plugin API is a build failure.
- The final Windows package includes a working QCA OpenSSL provider, SQLite driver, translations, official plugin DLLs, PsiMedia DLL, GStreamer modules and dependent DLLs.
- No release tag or stable-looking download link until CI and staged-package smoke tests pass.
