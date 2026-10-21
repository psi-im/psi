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

    # Psi is a release consumer of QCA even when its Iris submodule also
    # depends on it: record the direct SDK coordinate and verify it matches
    # Iris's independent lock instead of silently inheriting an unpinned binary.
    qca_direct = lock["qca"]
    require(qca_direct.get("repository") == "psi-im/qca"
            and qca_direct.get("lock_path") == iris["lock_path"]
            and isinstance(qca_direct.get("commit"), str)
            and HEX_SHA.fullmatch(qca_direct["commit"])
            and isinstance(qca_direct.get("release_tag"), str)
            and re.fullmatch(r"v\\d+\\.\\d+\\.\\d+", qca_direct["release_tag"])
            and isinstance(qca_direct.get("minimum_version"), str)
            and re.fullmatch(r"\\d+\\.\\d+\\.\\d+", qca_direct["minimum_version"]),
            "Invalid direct QCA dependency")
    for name in ("psimedia", "plugins"):
        dependency = lock[name]
        require(dependency.get("repository") == "psi-im/" + name
                and isinstance(dependency.get("commit"), str)
                and HEX_SHA.fullmatch(dependency["commit"])
                and (dependency.get("release_tag") is None or
                     (isinstance(dependency["release_tag"], str)
                      and TAG.fullmatch(dependency["release_tag"]))),
                "Invalid direct dependency: " + name)
    psimedia = lock["psimedia"]["commit"]
    qt = lock["qt"]
    require(qt["qt6"]["version"] == "6.12.0"
            and set(qt["qt6"]["platforms"]) ==
                {"windows-x64", "macos-arm64", "macos-x86_64",
                 "android-arm64-v8a", "android-x86_64", "android-x86"}
            and qt["qt5_windows_legacy"]["version"] == "5.15.2",
            "Inconsistent Qt6.12.0/legacy Qt5 release baseline")
    for name, source in lock["source_submodules"].items():
        require(name in ("3rdparty/qhttp", "3rdparty/qite", "3rdparty/http-parser")
                and re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", source["repository"])
                and HEX_SHA.fullmatch(source["commit"]), "Invalid source gitlink " + name)
    require(len(lock["source_submodules"]) == 3, "Three direct third-party source submodules required")
    require(lock["translations"]["repository"] == "psi-im/psi-l10n"
            and HEX_SHA.fullmatch(lock["translations"]["commit"]),
            "Invalid translation snapshot")
    require(lock["media_runtime"]["windows_modern_gstreamer"] == "1.28.7"
            and lock["media_runtime"]["windows_legacy_gstreamer"] == "1.24.13",
            "Invalid media runtime pin")
    keychain = lock["qtkeychain"]
    require(isinstance(keychain["tag"], str) and TAG.fullmatch(keychain["tag"]),
            "Invalid QtKeychain tag")
    require(isinstance(keychain["commit"], str) and HEX_SHA.fullmatch(keychain["commit"]),
            "Invalid QtKeychain commit")

    values = {
        "IRIS_RELEASE_TAG": iris["release_tag"] or "",
        "IRIS_MIN_VERSION": iris["minimum_version"],
        "QCA_TAG": qca_direct["release_tag"],
        "QCA_COMMIT": qca_direct["commit"],
        "QCA_MIN_VERSION": qca_direct["minimum_version"],
        "QT6_VERSION": qt["qt6"]["version"],
        "PSIMEDIA_SRTP_REF": psimedia,
        "PSIMEDIA_RELEASE_TAG": lock["psimedia"]["release_tag"] or "",
        "PLUGINS_REF": lock["plugins"]["commit"],
        "PLUGINS_RELEASE_TAG": lock["plugins"]["release_tag"] or "",
        "PSI_L10N_REF": lock["translations"]["commit"],
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
        require(qca_direct["release_tag"] == qca["tag"]
                and qca_direct["commit"] == qca["commit"]
                and qca_direct["minimum_version"] == qca["minimum_version"],
                "Direct Psi QCA pin disagrees with Iris's dependency lock")
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
