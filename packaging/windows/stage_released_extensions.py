#!/usr/bin/env python3
"""Stage verified, prebuilt Windows plugins and PsiMedia alongside Psi.

Psi is built alone (ENABLE_PLUGINS=OFF, BUILD_PSIMEDIA=OFF). The released
components are never compiled by the Psi packaging job.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path, PurePosixPath


def fail(message: str) -> None:
    raise ValueError(message)


def safe_relative(name: str) -> Path:
    part = PurePosixPath(name)
    if part.is_absolute() or not part.parts or any(p in (".", "..") for p in part.parts):
        fail(f"Unsafe relative artifact path: {name}")
    # Windows drive letters, backslashes and ADS must not enter an archive manifest.
    if ":" in name or "\\" in name:
        fail(f"Unsafe Windows artifact path: {name}")
    return Path(*part.parts)


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_checked(source: Path, destination: Path) -> None:
    if source.is_symlink() or not source.is_file():
        fail(f"Unexpected nonregular artifact file: {source}")
    if destination.exists():
        if hash_file(source) != hash_file(destination):
            fail(f"Conflicting prebuilt runtime/extension file: {destination}")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def stage_extensions(
    target: Path, plugins: Path, media: Path, dependency_lock: Path
) -> None:
    lock = json.loads(dependency_lock.read_text(encoding="utf-8"))
    qt_version = lock["qt"]["windows_msvc_qt6"]
    if qt_version != "6.12.0":
        fail(f"Unexpected locked Windows Qt version: {qt_version}")

    source = plugins / "PLUGINS-MANIFEST.json"
    if not source.is_file():
        fail(f"Missing released plugin manifest: {source}")
    manifest = json.loads(source.read_text(encoding="utf-8"))
    expected = {
        "platform": "windows-x64",
        "qt": qt_version,
        "plugins_commit": lock["plugins"]["commit"],
        "psi_api_commit": lock["plugins"].get("psi_api_commit", None),
        "qca_tag": lock["qca"]["tag"],
        "qca_commit": lock["qca"]["commit"],
    }
    # A valid SHA alone is insufficient: the independently built plugin package
    # must target the exact Psi Plugin API source locked by this Psi release.
    plugin_api_sha = expected["psi_api_commit"]
    if not isinstance(plugin_api_sha, str) or len(plugin_api_sha) != 40 or any(
        ch not in "0123456789abcdefABCDEF" for ch in plugin_api_sha
    ):
        fail("Psi dependency lock has invalid plugin API commit")
    for key, value in expected.items():
        if manifest.get(key) != value:
            fail(f"Released plugin {key} mismatch: {manifest.get(key)} != {value}")

    files = manifest.get("files")
    if not isinstance(files, dict) or len(files) < 20:
        fail("Released official plugin manifest contains too few binaries")
    binary_count = 0
    for name, digest in sorted(files.items()):
        rel = safe_relative(name)
        if rel.parts[0].lower() != "plugins" or rel.suffix.lower() != ".dll":
            fail(f"Unexpected plugin binary path: {name}")
        file = plugins / rel
        if not file.is_file():
            fail(f"Manifest plugin binary missing: {file}")
        if hash_file(file) != digest:
            fail(f"Released plugin checksum mismatch: {name}")
        binary_count += 1

    if binary_count < 20:
        fail("Too few official plugins")

    # Include exactly the declared source files and manifest, not unverified
    # leftovers in the downloaded artifact extraction directory.
    for name in sorted(files):
        relative = safe_relative(name)
        copy_checked(plugins / relative, target / relative)
    copy_checked(source, target / "PLUGINS-MANIFEST.json")

    # This is the *stage* directory contained in the published PsiMedia
    # GitHub Actions artifact zip, not its enclosing download directory.
    media_stage = media / "stage-msvc-modern"
    if not media_stage.is_dir():
        fail(f"PsiMedia release has no modern MSVC stage: {media_stage}")
    # Official PsiMedia installs its Windows payload under stage-msvc-modern/psi/.
    # Normalize that install prefix before merging the payload with Psi's
    # already-installed root; otherwise the plugin lands under psi/psi/plugins.
    media_payload = media_stage / "psi"
    if not media_payload.is_dir():
        fail(f"PsiMedia archive is missing its Psi install prefix: {media_payload}")
    media_plugins = list(media_payload.rglob("*mediaplugin.dll"))
    if len(media_plugins) != 1:
        fail(f"Expected one released media plugin, got {len(media_plugins)}")
    media_rel = media_plugins[0].relative_to(media_payload)
    if not media_rel.parts or media_rel.parts[0].lower() != "plugins":
        fail(f"Unexpected media plugin location: {media_rel}")
    # PsiMedia's independent Windows install has two roots:
    #   stage-msvc-modern/psi/plugins/mediaplugin.dll
    #   stage-msvc-modern/gstreamer-1.0/*.dll, gstreamer-runtime.txt
    # BIN_INSTALL_DIR is CMAKE_INSTALL_PREFIX, while PLUGINS_INSTALL_PATH
    # includes the Psi application directory. Only the latter prefix needs
    # stripping when overlaying an already-installed Psi package.
    gst = media_stage / "gstreamer-1.0"
    if not gst.is_dir() or not any(
        p.is_file() and p.suffix.lower() == ".dll" for p in gst.iterdir()
    ):
        fail("Prebuilt PsiMedia package lacks its GStreamer plugin runtime")
    gst_manifest = media_stage / "gstreamer-runtime.txt"
    if not gst_manifest.is_file():
        fail("Prebuilt PsiMedia GStreamer runtime manifest missing")

    media_hashes: dict[str, str] = {}
    for file in sorted(media_stage.rglob("*")):
        if file.is_symlink():
            fail(f"Unexpected symlink in PsiMedia release: {file}")
        if not file.is_file():
            continue
        relative = file.relative_to(media_stage)
        if relative.parts[0].lower() == "psi":
            if len(relative.parts) == 1:
                fail(f"Invalid PsiMedia path: {relative}")
            destination_relative = Path(*relative.parts[1:])
        else:
            destination_relative = relative
        media_hashes[relative.as_posix()] = hash_file(file)
        copy_checked(file, target / destination_relative)

    (target / "PSIMEDIA-SOURCE.json").write_text(
        json.dumps(
            {
                "repository": lock["psimedia"]["repository"],
                "tag": lock["psimedia"]["release_tag"],
                "commit": lock["psimedia"]["commit"],
                "qt": qt_version,
                "files": media_hashes,
            },
            indent=2,
        ) + "\n",
        encoding="utf-8",
    )
    print(f"Staged {binary_count} official plugins and PsiMedia with GStreamer")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--plugins", required=True, type=Path)
    parser.add_argument("--media", required=True, type=Path)
    parser.add_argument("--lock", default="dependencies.lock.json", type=Path)
    args = parser.parse_args()
    if not args.root.is_dir():
        fail(f"Psi stage not found: {args.root}")
    stage_extensions(args.root, args.plugins, args.media, args.lock)


if __name__ == "__main__":
    main()
