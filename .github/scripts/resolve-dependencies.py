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
    require(iris == {"source": "git-submodule", "path": "iris",
                     "lock_path": "iris/dependencies.lock.json"},
            "Iris must be an exact gitlink with its own dependency lock")

    psimedia = lock["psimedia"]["commit"]
    require(isinstance(psimedia, str) and HEX_SHA.fullmatch(psimedia), "Invalid psimedia SHA")
    keychain = lock["qtkeychain"]
    require(isinstance(keychain["tag"], str) and TAG.fullmatch(keychain["tag"]),
            "Invalid QtKeychain tag")
    require(isinstance(keychain["commit"], str) and HEX_SHA.fullmatch(keychain["commit"]),
            "Invalid QtKeychain commit")

    values = {
        "PSIMEDIA_SRTP_REF": psimedia,
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
        values.update(QCA_TAG=qca["tag"], QCA_COMMIT=qca["commit"],
                      QCA_MIN_VERSION=qca["minimum_version"])
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--require-iris", action="store_true")
    args = parser.parse_args()
    values = read_dependencies(args.workspace, require_iris=args.require_iris)
    output = "\n".join(f"{name}={value}" for name, value in values.items()) + "\n"
    for target in ("GITHUB_ENV", "GITHUB_OUTPUT"):
        if os.environ.get(target):
            with open(os.environ[target], "a", encoding="utf-8") as stream:
                stream.write(output)
    sys.stdout.write(output)


if __name__ == "__main__":
    try:
        main()
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        sys.exit(f"Invalid dependency lock: {error}")
