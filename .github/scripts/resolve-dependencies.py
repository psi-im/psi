#!/usr/bin/env python3
"""Resolve Psi's CI dependencies from one source-controlled lock file.

Iris itself is pinned by the Psi gitlink. Its QCA build revision is inherited
from the checked-out Iris dependency lock, never copied into Psi's lock.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

HEX_SHA = re.compile(r"[0-9a-f]{40}\Z")
TAG = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*\Z")
ASSET = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*\.zip\Z")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_dependencies(root, require_iris=False):
    lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    require(lock.get("schema_version") == 1, "Unsupported Psi dependency lock schema")
    iris = lock["iris"]
    require(isinstance(iris, dict)
            and set(iris) == {"source", "path", "lock_path", "release_tag", "minimum_version"}
            and iris["source"] == "git-submodule" and iris["path"] == "iris"
            and iris["lock_path"] == "iris/dependencies.lock.json",
            "Iris must be an exact gitlink with its own dependency lock")
    require(iris["release_tag"] is None or (isinstance(iris["release_tag"], str)
            and re.fullmatch(r"v\d+\.\d+(?:\.\d+){0,2}", iris["release_tag"])),
            "Invalid Iris release tag")
    # The release tag pins CI and packages; the minimum is the API floor for
    # builds against an installed Iris SDK (read by CMakeLists.txt).
    require(isinstance(iris["minimum_version"], str)
            and re.fullmatch(r"\d+\.\d+\.\d+", iris["minimum_version"]),
            "Invalid Iris minimum version")
    if iris["release_tag"] is not None:
        selected = tuple(int(part) for part in iris["release_tag"][1:].split("."))
        minimum = tuple(map(int, iris["minimum_version"].split(".")))
        selected += (0,) * (len(minimum) - len(selected))
        require(selected >= minimum, "Iris release tag below the minimum supported version")

    qca_direct = lock["qca"]
    require(qca_direct.get("repository") == "psi-im/qca"
            and qca_direct.get("source") == "released-sdk-shared-with-iris"
            and isinstance(qca_direct.get("tag"), str)
            and re.fullmatch(r"v\d+\.\d+\.\d+", qca_direct["tag"])
            and isinstance(qca_direct.get("commit"), str)
            and HEX_SHA.fullmatch(qca_direct["commit"]),
            "Invalid direct QCA release pin")
    psimedia_entry = lock["psimedia"]
    psimedia = psimedia_entry["commit"]
    require(psimedia_entry.get("repository") == "psi-im/psimedia"
            and isinstance(psimedia, str) and HEX_SHA.fullmatch(psimedia),
            "Invalid psimedia source pin")
    plugins_entry = lock["plugins"]
    require(plugins_entry.get("repository") == "psi-im/plugins"
            and isinstance(plugins_entry.get("commit"), str)
            and HEX_SHA.fullmatch(plugins_entry["commit"]),
            "Invalid plugin source pin")
    for name, entry in (("psimedia", psimedia_entry), ("plugins", plugins_entry)):
        tag = entry.get("release_tag")
        require(tag is None or isinstance(tag, str) and TAG.fullmatch(tag),
                "Invalid release tag for " + name)
    qt = lock["qt"]
    for platform in ("windows_msvc_qt6", "macos_qt6", "android_qt6"):
        require(qt.get(platform) == "6.12.0",
                "Qt 6.12.0 is required for " + platform)
    require(qt.get("windows_msys2_qt5") == "distribution-managed",
            "Do not hardcode an unavailable legacy Qt5 installer")
    translations = lock["translations"]
    require(translations.get("repository") == "psi-im/psi-l10n"
            and isinstance(translations.get("commit"), str)
            and HEX_SHA.fullmatch(translations["commit"]),
            "Invalid Psi translations source pin")
    submodules = lock["submodules"]
    require(submodules.get("source") == "gitlink"
            and sorted(submodules.get("paths", [])) == sorted((
                "iris", "3rdparty/qhttp", "3rdparty/http-parser", "3rdparty/qite")),
            "Invalid direct source gitlinks")
    keychain = lock["qtkeychain"]
    require(isinstance(keychain["tag"], str) and TAG.fullmatch(keychain["tag"]),
            "Invalid QtKeychain tag")
    require(isinstance(keychain["commit"], str) and HEX_SHA.fullmatch(keychain["commit"]),
            "Invalid QtKeychain commit")

    values = {
        "IRIS_RELEASE_TAG": iris["release_tag"] or "",
        "IRIS_MIN_VERSION": iris["minimum_version"],
        "PSIMEDIA_SRTP_REF": psimedia,
        "PSIMEDIA_RELEASE_TAG": psimedia_entry["release_tag"] or "",
        "PLUGINS_REF": plugins_entry["commit"],
        "PLUGINS_RELEASE_TAG": plugins_entry["release_tag"] or "",
        "PSI_L10N_REF": translations["commit"],
        "QT6_RELEASE_VERSION": qt["windows_msvc_qt6"],
        "QTKEYCHAIN_TAG": keychain["tag"],
        "QTKEYCHAIN_COMMIT": keychain["commit"],
    }
    for key, prefix in (("msvc_qt6", "MSVC_QT6"), ("msys2_qt5", "MSYS2_QT5")):
        sdk = lock["windows_sdks"][key]
        tag, asset = sdk["tag"], sdk["archive"]
        require(isinstance(tag, str) and TAG.fullmatch(tag), "Invalid SDK tag: " + key)
        require(isinstance(asset, str) and ASSET.fullmatch(asset), "Invalid SDK archive: " + key)
        values[prefix + "_SDK_TAG"] = tag
        values[prefix + "_SDK_ARCHIVE"] = asset

    iris_lock = root / iris["lock_path"]
    if require_iris:
        require(iris_lock.is_file(), "Initialize the pinned Iris submodule first")
    if iris_lock.is_file():
        qca = json.loads(iris_lock.read_text(encoding="utf-8"))["qca"]
        require(isinstance(qca["commit"], str) and HEX_SHA.fullmatch(qca["commit"]),
                "Invalid Iris QCA commit")
        require(isinstance(qca["tag"], str) and re.fullmatch(r"v\d+\.\d+\.\d+", qca["tag"]),
                "Invalid Iris QCA tag")
        require(isinstance(qca["minimum_version"], str)
                and re.fullmatch(r"\d+\.\d+\.\d+", qca["minimum_version"]),
                "Invalid Iris QCA minimum version")
        selected = tuple(map(int, qca["tag"][1:].split(".")))
        minimum = tuple(map(int, qca["minimum_version"].split(".")))
        require(selected >= minimum, "Iris QCA tag below the minimum supported version")
        require(qca_direct["tag"] == qca["tag"]
                and qca_direct["commit"] == qca["commit"],
                "Psi direct QCA pin differs from Iris transitive QCA lock")
        values.update(QCA_TAG=qca["tag"], QCA_COMMIT=qca["commit"],
                      QCA_MIN_VERSION=qca["minimum_version"])
    return values


def verify_iris_release(root, tag):
    """The release is an SDK coordinate; the gitlink remains the source pin."""
    entry = subprocess.check_output(
        ["git", "ls-tree", "HEAD", "--", "iris"], cwd=root, text=True, timeout=30).split()
    require(len(entry) == 4 and entry[0:2] == ["160000", "commit"]
            and HEX_SHA.fullmatch(entry[2]) and entry[3] == "iris",
            "Cannot resolve the pinned Iris gitlink")
    if not tag:
        return  # Explicit source-only revision; there is no compatible released SDK.
    ref = "refs/tags/" + tag
    output = subprocess.check_output(
        ["git", "ls-remote", "--tags", "https://github.com/psi-im/iris.git", ref, ref + "^{}"],
        cwd=root, text=True, timeout=30)
    refs = {}
    for line in output.splitlines():
        fields = line.split()
        require(len(fields) == 2 and HEX_SHA.fullmatch(fields[0]), "Invalid Iris tag response")
        refs[fields[1]] = fields[0]
    resolved = refs.get(ref + "^{}", refs.get(ref))
    require(resolved is not None, "Locked Iris release tag does not exist: " + tag)
    require(resolved == entry[2], "Locked Iris release " + tag + " does not match the Psi gitlink")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--require-iris", action="store_true")
    parser.add_argument("--verify-iris-release", action="store_true",
                        help="Verify the remote release tag against Psi's gitlink (requires network)")
    args = parser.parse_args()
    values = read_dependencies(args.workspace, require_iris=args.require_iris)
    if args.verify_iris_release:
        # Strict release matching applies only to Psi release-tag builds.
        psi_tag = os.environ.get("GITHUB_REF", "")
        is_release = (psi_tag.startswith("refs/tags/")
                      and re.fullmatch(r"\d+\.\d+(?:[._-][A-Za-z0-9.]+)*",
                                       psi_tag.removeprefix("refs/tags/")))
        if is_release:
            verify_iris_release(args.workspace, values["IRIS_RELEASE_TAG"])
    output = "\n".join(f"{name}={value}" for name, value in values.items()) + "\n"
    for target in ("GITHUB_ENV", "GITHUB_OUTPUT"):
        if os.environ.get(target):
            with open(os.environ[target], "a", encoding="utf-8") as stream:
                stream.write(output)
    sys.stdout.write(output)


if __name__ == "__main__":
    try:
        main()
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        sys.exit(f"Invalid dependency lock: {error}")
