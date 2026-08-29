#!/usr/bin/env python3
"""Prepare a self-contained Psi MSVC/Qt6 Windows runtime tree."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

DLL_LINE_RE = re.compile(r"^\s*([A-Za-z0-9_.+\-]+\.dll)\s*$", re.IGNORECASE)
MSVC_RUNTIME_RE = re.compile(
    r"^(?:vcruntime\d.*|msvcp\d.*|concrt\d.*)\.dll$",
    re.IGNORECASE,
)


def run(cmd: list[str]) -> str:
    proc = subprocess.run(
        cmd,
        check=True,
        text=True,
        capture_output=True,
        errors="replace",
    )
    return proc.stdout + proc.stderr


def imports(dumpbin: Path, binary: Path) -> list[str]:
    output = run([str(dumpbin), "/DEPENDENTS", str(binary)])
    deps: set[str] = set()
    in_dependencies = False

    for line in output.splitlines():
        if "Image has the following dependencies:" in line:
            in_dependencies = True
            continue
        if not in_dependencies:
            continue
        if line.strip().startswith("Summary"):
            break
        match = DLL_LINE_RE.match(line)
        if match:
            deps.add(match.group(1))

    return sorted(deps, key=str.lower)


def is_pe64(dumpbin: Path, binary: Path) -> bool:
    output = run([str(dumpbin), "/HEADERS", str(binary)]).lower()
    return "8664 machine (x64)" in output or "machine (x64)" in output


def find_case_insensitive(directory: Path, filename: str) -> Path | None:
    if not directory.is_dir():
        return None
    wanted = filename.lower()
    for child in directory.iterdir():
        if child.is_file() and child.name.lower() == wanted:
            return child
    return None


def find_recursive_case_insensitive(directory: Path, filename: str) -> Path | None:
    if not directory.is_dir():
        return None
    wanted = filename.lower()
    for child in directory.rglob("*"):
        if child.is_file() and child.name.lower() == wanted:
            return child
    return None


def classify_system_dll(system32: Path, name: str) -> bool:
    lowered = name.lower()
    if lowered.startswith(("api-ms-win-", "ext-ms-win-")):
        return True
    return find_case_insensitive(system32, name) is not None


def copy_runtime_data(source_root: Path, root: Path) -> None:
    myspell = source_root / "myspell"
    if myspell.is_dir():
        target = root / "myspell"
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(myspell, target)


def copy_sdk_runtime(sdk: Path, root: Path) -> None:
    sdk_bin = sdk / "bin"
    if not sdk_bin.is_dir():
        raise SystemExit(f"SDK bin directory is missing: {sdk_bin}")

    for dll in sorted(sdk_bin.glob("*.dll")):
        shutil.copy2(dll, root / dll.name)


def prune_unused_qt_plugins(root: Path) -> None:
    # Qt deployment brings all installed SQL drivers when QtSql is linked.
    # Psi only uses SQLite; keeping other drivers needlessly expands the
    # dependency closure with database client runtimes.
    sqldrivers = root / "qtplugins" / "sqldrivers"
    if not sqldrivers.is_dir():
        raise SystemExit(f"Qt SQL plugin directory is missing: {sqldrivers}")

    qsqlite = find_case_insensitive(sqldrivers, "qsqlite.dll")
    if not qsqlite:
        raise SystemExit(f"Required Qt SQLite plugin is missing from {sqldrivers}")

    for plugin in sqldrivers.glob("*.dll"):
        if plugin.name.lower() != "qsqlite.dll":
            plugin.unlink()


def collect_pe_files(root: Path) -> list[Path]:
    return sorted(
        (
            path
            for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() in {".exe", ".dll"}
        ),
        key=lambda path: str(path).lower(),
    )


def locate_dumpbin() -> Path:
    candidate = shutil.which("dumpbin.exe") or shutil.which("dumpbin")
    if candidate:
        return Path(candidate).resolve()
    raise SystemExit("dumpbin.exe is not available in the MSVC environment")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--qt-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--qt-conf", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    sdk = args.sdk.resolve()
    qt_root = args.qt_root.resolve()
    source_root = args.source_root.resolve()
    qt_conf = args.qt_conf.resolve()
    report = args.report.resolve()

    psi_exe = root / "psi.exe"
    if not psi_exe.is_file():
        raise SystemExit(f"Installed Psi executable is missing: {psi_exe}")

    windeployqt = qt_root / "bin" / "windeployqt.exe"
    if not windeployqt.is_file():
        raise SystemExit(f"windeployqt is missing: {windeployqt}")

    dumpbin = locate_dumpbin()
    qtplugins = root / "qtplugins"
    translations = root / "translations"
    qtplugins.mkdir(parents=True, exist_ok=True)
    translations.mkdir(parents=True, exist_ok=True)

    # Official Qt/MSVC deployment is authoritative for Qt, WebEngine,
    # QtWebEngineProcess and resources/locales. Compiler runtime deployment is
    # disabled here because the dependency closure below handles app-local DLLs
    # explicitly and the NSIS path can install the official VC redistributable.
    subprocess.run(
        [
            str(windeployqt),
            "--release",
            "--verbose",
            "2",
            "--no-compiler-runtime",
            "--dir",
            str(root),
            "--libdir",
            str(root),
            "--plugindir",
            str(qtplugins),
            "--translationdir",
            str(translations),
            str(psi_exe),
        ],
        check=True,
    )

    prune_unused_qt_plugins(root)
    shutil.copy2(qt_conf, root / "qt.conf")
    copy_runtime_data(source_root, root)
    copy_sdk_runtime(sdk, root)

    system32 = Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32"
    search_dirs = [root, sdk / "bin", qt_root / "bin"]

    redist_x64: Path | None = None
    redist_env = os.environ.get("VCToolsRedistDir")
    if redist_env:
        candidate = Path(redist_env) / "x64"
        if candidate.is_dir():
            redist_x64 = candidate

    copied: dict[str, str] = {}
    system: set[str] = set()
    unresolved: set[str] = set()
    msvc_runtime: set[str] = set()

    while True:
        changed = False
        for binary in collect_pe_files(root):
            if not is_pe64(dumpbin, binary):
                raise SystemExit(f"Non-x64 PE file in x64 package: {binary}")

            for dll_name in imports(dumpbin, binary):
                lowered = dll_name.lower()
                if MSVC_RUNTIME_RE.match(dll_name):
                    msvc_runtime.add(dll_name)

                if find_case_insensitive(
                    binary.parent, dll_name
                ) or find_case_insensitive(root, dll_name):
                    continue

                candidate = None
                for directory in search_dirs:
                    candidate = find_case_insensitive(directory, dll_name)
                    if candidate:
                        break

                if not candidate and redist_x64:
                    candidate = find_recursive_case_insensitive(redist_x64, dll_name)

                if candidate:
                    if not is_pe64(dumpbin, candidate):
                        raise SystemExit(
                            f"Non-x64 runtime candidate for {dll_name}: {candidate}"
                        )
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
        for dll_name in imports(dumpbin, binary):
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
        "Psi Windows MSVC runtime dependency report\n"
        "==========================================\n\n"
        "Automatically copied runtime DLLs:\n"
        + "".join(
            f"  {name} <- {source}\n" for name, source in sorted(copied.items())
        )
        + "\nSystem DLL imports:\n"
        + "".join(f"  {name}\n" for name in sorted(system, key=str.lower))
        + "\nMSVC runtime imports:\n"
        + (
            "".join(
                f"  {name}\n" for name in sorted(msvc_runtime, key=str.lower)
            )
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
        raise SystemExit("Unresolved runtime DLL imports remain in the MSVC package")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
