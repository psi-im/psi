#!/usr/bin/env python3
"""Generate safe NSIS uninstall commands for an already staged payload tree."""

from __future__ import annotations

import argparse
from pathlib import Path


def nsis_escape(value: str) -> str:
    return value.replace("$", "$$").replace('"', '$\\\"')


def win_rel(path: Path) -> str:
    return str(path).replace("/", "\\")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    if not root.is_dir():
        raise SystemExit(f"Payload root does not exist: {root}")

    files = sorted(
        (p.relative_to(root) for p in root.rglob("*") if p.is_file()),
        key=lambda p: (len(p.parts), str(p).lower()),
        reverse=True,
    )
    dirs = sorted(
        (p.relative_to(root) for p in root.rglob("*") if p.is_dir()),
        key=lambda p: (len(p.parts), str(p).lower()),
        reverse=True,
    )

    lines = [
        "; Generated from the exact release staging tree. Do not edit.",
        "; Only files installed by this package are deleted.",
        "",
    ]
    for rel in files:
        lines.append(f'Delete "$INSTDIR\\{nsis_escape(win_rel(rel))}"')
    lines.append("")
    for rel in dirs:
        lines.append(f'RMDir "$INSTDIR\\{nsis_escape(win_rel(rel))}"')
    lines.append("")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
