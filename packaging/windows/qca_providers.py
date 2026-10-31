#!/usr/bin/env python3
"""Select and stage Windows QCA providers from the dependency SDK."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil

PROVIDER_DLL = re.compile(
    r"^(?:lib)?qca(?:3)?-(wincrypto|ossl|cyrus-sasl|wingss)\.dll$", re.IGNORECASE
)
STAGED_PROVIDER_DLL = re.compile(
    r"^(?:lib)?qca(?:3)?-(?:wincrypto|ossl|cyrus-sasl|wingss)d?\.dll$", re.IGNORECASE
)


def select_qca_providers(sdk: Path, required: tuple[str, ...] = ()) -> list[Path]:
    providers: dict[str, Path] = {}
    for path in sdk.rglob("*"):
        match = PROVIDER_DLL.fullmatch(path.name)
        if not match or not path.is_file():
            continue
        name = match.group(1).lower()
        if name in providers:
            raise SystemExit(f"Ambiguous QCA {name} provider in SDK: {providers[name]} and {path}")
        providers[name] = path
    missing = sorted(set(required) - providers.keys())
    if missing:
        raise SystemExit(f"Required QCA providers missing from SDK: {', '.join(missing)}")
    primary = "wincrypto" if "wincrypto" in providers else "ossl"
    if primary not in providers:
        raise SystemExit("SDK contains neither the native nor the OpenSSL QCA crypto provider")
    return [providers[name] for name in (primary, "cyrus-sasl", "wingss") if name in providers]


def stage_qca_providers(sdk: Path, root: Path, required: tuple[str, ...] = ()) -> None:
    sdk, root = sdk.resolve(), root.resolve()
    if sdk == root or sdk in root.parents or root in sdk.parents:
        raise SystemExit("SDK and package directories must not overlap")
    providers = select_qca_providers(sdk, required)
    destination = root / "qtplugins" / "crypto"
    # Keep one provider location and remove previous SDK copies before restaging.
    for directory in (destination, root / "crypto"):
        if directory.is_dir():
            for path in directory.iterdir():
                if STAGED_PROVIDER_DLL.fullmatch(path.name) and path.is_file():
                    path.unlink()
    destination.mkdir(parents=True, exist_ok=True)
    for provider in providers:
        shutil.copy2(provider, destination / provider.name)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--require", nargs="*", default=())
    args = parser.parse_args()
    for provider in select_qca_providers(args.sdk, tuple(args.require)):
        print(f"QCA provider: {provider.name}")


if __name__ == "__main__":
    main()
