#!/usr/bin/env bash
# Restore legacy MINGW64 packages removed from MSYS2's sync database.
# Keep the legacy Windows 7 CI profile working without migrating it to UCRT64.
set -euo pipefail

package_dir="$(mktemp -d)"
trap 'rm -rf "$package_dir"' EXIT

base_url="https://repo.msys2.org/mingw/mingw64"
packages=(
  "mingw-w64-x86_64-qt5-multimedia-5.15.19-1-any.pkg.tar.zst"
  "mingw-w64-x86_64-qt5-tools-5.15.19-2-any.pkg.tar.zst"
  "mingw-w64-x86_64-qt5-winextras-5.15.19-1-any.pkg.tar.zst"
  "mingw-w64-x86_64-qt5-svg-5.15.19+kde+r5-1-any.pkg.tar.zst"
  "mingw-w64-x86_64-usrsctp-0.9.5.0-3-any.pkg.tar.zst"
)

local_paths=()
for package in "${packages[@]}"; do
  curl --fail --location --show-error --silent --retry 3 \
    "$base_url/$package" --output "$package_dir/$package"
  # Keep the official detached signature alongside each local package.
  # pacman will check it against MSYS2's trusted keyring during installation.
  curl --fail --location --show-error --silent --retry 3 \
    "$base_url/$package.sig" --output "$package_dir/$package.sig"
  local_paths+=("$package_dir/$package")
done

cat > "$package_dir/SHA256SUMS" <<'SHA256'
18141c05175522df437224cdf56df0673161426931a66779c2cf6506731427c4  mingw-w64-x86_64-qt5-multimedia-5.15.19-1-any.pkg.tar.zst
a96e2a38efe9ecba3ef9164b21bfc758c9b7dc0db7de061002f61398e0d00b95  mingw-w64-x86_64-qt5-tools-5.15.19-2-any.pkg.tar.zst
55c62783072575ffbbdedc40573c12de2b1317a1583d4f3c4672ef4dcd3b70f8  mingw-w64-x86_64-qt5-winextras-5.15.19-1-any.pkg.tar.zst
7e7859ddd92ce65ad38c8ea2ab2917d33a211ce2cff43207f7cadc15c99a2d3e  mingw-w64-x86_64-qt5-svg-5.15.19+kde+r5-1-any.pkg.tar.zst
SHA256

(
  cd "$package_dir"
  sha256sum --check SHA256SUMS
)
# The default LocalFileSigLevel can allow unsigned packages. Enforce a
# trusted signature for every restored archive without modifying system
# pacman.conf or using pacman-key (which requires elevated permissions).
awk '
  /^\[options\]/ {
    print
    print "LocalFileSigLevel = Required TrustedOnly"
    in_options = 1
    next
  }
  /^\[/ { in_options = 0 }
  in_options && /^[[:space:]]*#?[[:space:]]*LocalFileSigLevel[[:space:]]*=/ { next }
  { print }
' /etc/pacman.conf > "$package_dir/pacman.conf"
pacman --config "$package_dir/pacman.conf" --noconfirm -U --needed "${local_paths[@]}"
