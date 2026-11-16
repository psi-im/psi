#!/usr/bin/env bash
# Restore Qt5 MINGW64 packages removed from MSYS2's sync database.
# Keep the legacy Windows 7 CI profile working without migrating it to UCRT64.
set -euo pipefail

package_dir="$(mktemp -d)"
trap 'rm -rf "$package_dir"' EXIT

base_url="https://repo.msys2.org/mingw/mingw64"
packages=(
  "mingw-w64-x86_64-qt5-multimedia-5.15.19-1-any.pkg.tar.zst"
  "mingw-w64-x86_64-qt5-tools-5.15.19-2-any.pkg.tar.zst"
  "mingw-w64-x86_64-qt5-winextras-5.15.19-1-any.pkg.tar.zst"
)

local_paths=()
for package in "${packages[@]}"; do
  curl --fail --location --show-error --silent --retry 3 \
    "$base_url/$package" --output "$package_dir/$package"
  local_paths+=("$package_dir/$package")
done

cat > "$package_dir/SHA256SUMS" <<'SHA256'
18141c05175522df437224cdf56df0673161426931a66779c2cf6506731427c4  mingw-w64-x86_64-qt5-multimedia-5.15.19-1-any.pkg.tar.zst
a96e2a38efe9ecba3ef9164b21bfc758c9b7dc0db7de061002f61398e0d00b95  mingw-w64-x86_64-qt5-tools-5.15.19-2-any.pkg.tar.zst
55c62783072575ffbbdedc40573c12de2b1317a1583d4f3c4672ef4dcd3b70f8  mingw-w64-x86_64-qt5-winextras-5.15.19-1-any.pkg.tar.zst
SHA256

(
  cd "$package_dir"
  sha256sum --check SHA256SUMS
)
pacman --noconfirm -U --needed "${local_paths[@]}"
