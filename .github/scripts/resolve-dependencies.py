#!/usr/bin/env python3
"""Resolve Psi's CI dependencies from one source-controlled lock file.

Iris itself is pinned by the Psi gitlink, and its release is whatever tag that
gitlink has. Its QCA build revision is inherited from the checked-out Iris
dependency lock; the copy in Psi's lock serves release packaging and has to match.
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
VERSION = re.compile(r"\d+\.\d+\.\d+\Z")
IRIS_RELEASE = re.compile(r"v\d+\.\d+(?:\.\d+){0,2}\Z")
IRIS_LOCK = "iris/dependencies.lock.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_dependencies(root, require_iris=False):
    lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    iris = lock["iris"]
    # The API floor for builds against an installed Iris SDK (read by CMakeLists.txt).
    # The source revision and the release tag are those of the gitlink, not lock data.
    require(isinstance(iris, dict) and set(iris) == {"minimum_version"},
            "Iris must be an exact gitlink with its own dependency lock")
    require(isinstance(iris["minimum_version"], str) and VERSION.fullmatch(iris["minimum_version"]),
            "Invalid Iris minimum version")

    qca_direct = lock["qca"]
    require(isinstance(qca_direct.get("tag"), str)
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
    # Official plugins are consumed only by all-in-one release packaging.
    # The release tag selects the archive; do not pin its mutable source SHA.
    require(isinstance(plugins_entry, dict)
            and set(plugins_entry) == {"release_tag"}
            and isinstance(plugins_entry["release_tag"], str)
            and TAG.fullmatch(plugins_entry["release_tag"]),
            "Invalid official plugin release tag")
    tag = psimedia_entry.get("release_tag")
    require(tag is None or isinstance(tag, str) and TAG.fullmatch(tag),
            "Invalid release tag for psimedia")
    qt = lock["qt"]
    require(isinstance(qt.get("windows_msvc_qt6"), str) and VERSION.fullmatch(qt["windows_msvc_qt6"]),
            "Invalid Windows Qt version")
    translations = lock["translations"]
    require(isinstance(translations.get("commit"), str) and HEX_SHA.fullmatch(translations["commit"]),
            "Invalid Psi translations source pin")
    keychain = lock["qtkeychain"]
    require(isinstance(keychain["tag"], str) and TAG.fullmatch(keychain["tag"]),
            "Invalid QtKeychain tag")
    require(isinstance(keychain["commit"], str) and HEX_SHA.fullmatch(keychain["commit"]),
            "Invalid QtKeychain commit")

    values = {
        "PSIMEDIA_SRTP_REF": psimedia,
        "PSIMEDIA_RELEASE_TAG": psimedia_entry["release_tag"] or "",
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

    iris_lock = root / IRIS_LOCK
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


def resolve_iris_release(root):
    """Return the release tag of Psi's exact Iris gitlink, or "" without one.

    A source-only Iris revision is normal: it must never consume another
    release's libraries, so Psi then builds that exact gitlink in-tree. The same
    goes for a lookup that fails, which is why this never fails a build.
    """
    try:
        entry = subprocess.check_output(
            ["git", "ls-tree", "HEAD", "--", "iris"],
            cwd=root, text=True, timeout=30).split()
        require(len(entry) == 4 and entry[0:2] == ["160000", "commit"]
                and HEX_SHA.fullmatch(entry[2]) and entry[3] == "iris",
                "Cannot resolve the pinned Iris gitlink")

        output = subprocess.check_output(
            ["git", "ls-remote", "--tags", "https://github.com/psi-im/iris.git"],
            cwd=root, text=True, timeout=30)
        refs = {}
        for line in output.splitlines():
            fields = line.split()
            require(len(fields) == 2 and HEX_SHA.fullmatch(fields[0]),
                    "Invalid Iris tag response")
            refs[fields[1]] = fields[0]

        # An annotated tag also lists the commit it peels to.
        released = []
        for ref, sha in refs.items():
            if not ref.startswith("refs/tags/") or ref.endswith("^{}"):
                continue
            name = ref[len("refs/tags/"):]
            if IRIS_RELEASE.fullmatch(name) and refs.get(ref + "^{}", sha) == entry[2]:
                released.append(name)
        return max(released, key=lambda name: tuple(map(int, name[1:].split("."))), default="")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print("Iris release lookup unavailable or inconsistent; using bundled Iris: "
              + str(error), file=sys.stderr)
        return ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--require-iris", action="store_true")
    parser.add_argument("--resolve-iris-release", action="store_true",
                        help="Also look up the release tag of the exact Iris gitlink, if it has one")
    args = parser.parse_args()
    values = read_dependencies(args.workspace, require_iris=args.require_iris)
    if args.resolve_iris_release:
        values["IRIS_RELEASE_TAG"] = resolve_iris_release(args.workspace)
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
