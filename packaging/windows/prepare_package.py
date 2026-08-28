#!/usr/bin/env python3
"""Prepare a self-contained Psi Windows runtime tree."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

DLL_RE = re.compile(r"^\s*DLL Name:\s*(.+?)\s*$", re.IGNORECASE | re.MULTILINE)
MSVC_RUNTIME_RE = re.compile(
    r"^(?:vcruntime\d.*|msvcp\d.*|concrt\d.*)\.dll$",
    re.IGNORECASE,
)

# This deliberately remains conservative. It catches obvious post-Win7 DLL and
# API-set dependencies, while a real Windows 7 SP1 smoke test remains the final
# compatibility proof.
WIN7_UNAVAILABLE_DLLS = {
    "bcryptprimitives.dll",
    "combase.dll",
    "coremessaging.dll",
    "d3d12.dll",
    "dcomp.dll",
    "dxcore.dll",
    "kernel.appcore.dll",
    "onecoreuap.dll",
    "shcore.dll",
    "twinapi.appcore.dll",
    "win32u.dll",
    "windows.ui.dll",
}
WIN7_UNAVAILABLE_PREFIXES = (
    "api-ms-win-appmodel-",
    "api-ms-win-core-winrt-",
    "api-ms-win-shcore-",
)


def run(cmd: list[str]) -> str:
    proc = subprocess.run(cmd, check=True, text=True, capture_output=True)
    return proc.stdout + proc.stderr


def imports(objdump: Path, binary: Path) -> list[str]:
    output = run([str(objdump), "-p", str(binary)])
    return sorted(set(DLL_RE.findall(output)), key=str.lower)


def is_pe64(objdump: Path, binary: Path) -> bool:
    return "pei-x86-64" in run([str(objdump), "-f", str(binary)]).lower()


def find_case_insensitive(directory: Path, filename: str) -> Path | None:
    if not directory.is_dir():
        return None
    wanted = filename.lower()
    for child in directory.iterdir():
        if child.is_file() and child.name.lower() == wanted:
            return child
    return None


def classify_system_dll(system32: Path, name: str) -> bool:
    lowered = name.lower()
    if lowered.startswith(("api-ms-win-", "ext-ms-win-")):
        return True
    return find_case_insensitive(system32, name) is not None


def copy_sdk_runtime(sdk: Path, root: Path) -> None:
    sdk_bin = sdk / "bin"
    if not sdk_bin.is_dir():
        raise SystemExit(f"SDK bin directory is missing: {sdk_bin}")

    for dll in sorted(sdk_bin.glob("*.dll")):
        shutil.copy2(dll, root / dll.name)

    crypto_dir = root / "qtplugins" / "crypto"
    crypto_dir.mkdir(parents=True, exist_ok=True)
    qca_plugins = sorted(
        p for p in sdk.rglob("*.dll") if "qca-ossl" in p.name.lower()
    )
    if not qca_plugins:
        raise SystemExit("QCA OpenSSL provider DLL was not found in the SDK")
    for plugin in qca_plugins:
        shutil.copy2(plugin, crypto_dir / plugin.name)


def copy_runtime_data(source_root: Path, root: Path) -> None:
    myspell = source_root / "myspell"
    if myspell.is_dir():
        target = root / "myspell"
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(myspell, target)


def collect_pe_files(root: Path) -> list[Path]:
    return sorted(
        (
            p
            for p in root.rglob("*")
            if p.is_file() and p.suffix.lower() in {".exe", ".dll"}
        ),
        key=lambda p: str(p).lower(),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--mingw-prefix", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--qt-conf", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument(
        "--profile", choices=("modern", "legacy-win7"), required=True
    )
    args = parser.parse_args()

    root = args.root.resolve()
    sdk = args.sdk.resolve()
    mingw = args.mingw_prefix.resolve()
    source_root = args.source_root.resolve()
    qt_conf = args.qt_conf.resolve()
    report = args.report.resolve()

    psi_exe = root / "psi.exe"
    if not psi_exe.is_file():
        raise SystemExit(f"Installed Psi executable is missing: {psi_exe}")

    windeployqt = mingw / "bin" / "windeployqt.exe"
    objdump = mingw / "bin" / "objdump.exe"
    if not windeployqt.is_file():
        raise SystemExit(f"windeployqt is missing: {windeployqt}")
    if not objdump.is_file():
        raise SystemExit(f"objdump is missing: {objdump}")

    qtplugins = root / "qtplugins"
    translations = root / "translations"
    qtplugins.mkdir(parents=True, exist_ok=True)
    translations.mkdir(parents=True, exist_ok=True)

    # Psi translations are built and installed by CMake from psi-l10n.  The
    # MSYS2 Qt5 windeployqt does not support the upstream --translationdir
    # option, so deploy only Qt runtime libraries/plugins here and leave the
    # already installed Psi translations untouched.
    #
    # MSYS2 builds Qt5 with desktop OpenGL and keeps ANGLE as an optional
    # package, so asking windeployqt to deploy ANGLE otherwise fails on the
    # absent libGLESv2.dll.  Compiler runtime deployment is disabled as well:
    # the recursive PE dependency closure below is authoritative for MinGW DLLs.
    subprocess.run(
        [
            str(windeployqt),
            "--release",
            "--verbose",
            "2",
            "--no-angle",
            "--no-compiler-runtime",
            "--dir",
            str(root),
            "--libdir",
            str(root),
            "--plugindir",
            str(qtplugins),
            str(psi_exe),
        ],
        check=True,
    )

    shutil.copy2(qt_conf, root / "qt.conf")
    copy_runtime_data(source_root, root)
    copy_sdk_runtime(sdk, root)

    search_dirs = [root, sdk / "bin", mingw / "bin"]
    system32 = Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32"
    copied: dict[str, str] = {}
    system: set[str] = set()
    unresolved: set[str] = set()
    msvc_runtime: set[str] = set()
    post_win7: set[str] = set()

    while True:
        changed = False
        for binary in collect_pe_files(root):
            if not is_pe64(objdump, binary):
                raise SystemExit(f"Non-x64 PE file in x64 package: {binary}")

            for dll_name in imports(objdump, binary):
                lowered = dll_name.lower()
                if MSVC_RUNTIME_RE.match(dll_name):
                    msvc_runtime.add(dll_name)
                if lowered in WIN7_UNAVAILABLE_DLLS or lowered.startswith(
                    WIN7_UNAVAILABLE_PREFIXES
                ):
                    post_win7.add(dll_name)

                if find_case_insensitive(
                    binary.parent, dll_name
                ) or find_case_insensitive(root, dll_name):
                    continue

                candidate = None
                for directory in search_dirs:
                    candidate = find_case_insensitive(directory, dll_name)
                    if candidate:
                        break

                if candidate:
                    shutil.copy2(candidate, root / candidate.name)
                    copied[lowered] = str(candidate)
                    changed = True
                    continue

                if classify_system_dll(system32, dll_name):
                    system.add(dll_name)
                else:
                    unresolved.add(dll_name)

        if not changed:
            break
        unresolved.clear()

    for binary in collect_pe_files(root):
        for dll_name in imports(objdump, binary):
            if find_case_insensitive(
                binary.parent, dll_name
            ) or find_case_insensitive(root, dll_name):
                continue
            if classify_system_dll(system32, dll_name):
                system.add(dll_name)
            else:
                unresolved.add(dll_name)

    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(
        "Psi Windows runtime dependency report\n"
        "=====================================\n"
        f"Profile: {args.profile}\n\n"
        "Automatically copied runtime DLLs:\n"
        + "".join(
            f"  {name} <- {source}\n" for name, source in sorted(copied.items())
        )
        + "\nSystem DLL imports:\n"
        + "".join(f"  {name}\n" for name in sorted(system, key=str.lower))
        + "\nKnown post-Windows-7 DLL/API-set imports:\n"
        + (
            "".join(f"  {name}\n" for name in sorted(post_win7, key=str.lower))
            or "  none\n"
        )
        + "\nMSVC runtime imports:\n"
        + (
            "".join(f"  {name}\n" for name in sorted(msvc_runtime, key=str.lower))
            or "  none\n"
        )
        + "\nUnresolved imports:\n"
        + (
            "".join(f"  {name}\n" for name in sorted(unresolved, key=str.lower))
            or "  none\n"
        ),
        encoding="utf-8",
    )

    if unresolved:
        print(report.read_text(encoding="utf-8"), file=sys.stderr)
        raise SystemExit("Unresolved runtime DLL imports remain in the package")

    if args.profile == "legacy-win7" and post_win7:
        print(report.read_text(encoding="utf-8"), file=sys.stderr)
        raise SystemExit("Known post-Windows-7 DLL imports detected in legacy package")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
