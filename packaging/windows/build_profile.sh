#!/usr/bin/env bash
set -euo pipefail

profile=${1:?usage: build_profile.sh modern|legacy-win7 SDK_DIR OUTPUT_DIR}
sdk_dir=$(cd "${2:?SDK_DIR is required}" && pwd)
output_dir=$(mkdir -p "${3:?OUTPUT_DIR is required}" && cd "$3" && pwd)
source_dir=$(pwd)

export CCACHE_DIR="$source_dir/.ccache"
export CCACHE_BASEDIR="$source_dir"
mkdir -p "$CCACHE_DIR"
ccache --max-size=1G
ccache --zero-stats

case "$profile" in
    modern)
        winver=0x0A00
        package_os=win10
        nsis_profile=()
        ;;
    legacy-win7)
        winver=0x0601
        package_os=win7
        nsis_profile=(-DLEGACY_WIN7)
        ;;
    *)
        echo "Unknown Windows package profile: $profile" >&2
        exit 2
        ;;
esac

build_dir="$source_dir/build-msys2-$profile"
stage_dir="$source_dir/package-msys2-$profile"
l10n_dir="$source_dir/.packaging-psi-l10n-$profile"
report="$output_dir/dependencies-$profile.txt"
rm -rf "$build_dir" "$stage_dir" "$l10n_dir"

# Psi translations intentionally live in the separate psi-l10n repository.
# Use a shallow sparse checkout so release packages contain current translations
# without restoring the old installer-side download machinery.
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/psi-im/psi-l10n.git "$l10n_dir"
git -C "$l10n_dir" sparse-checkout set translations
l10n_revision=$(git -C "$l10n_dir" rev-parse HEAD)

cmake -S . -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$stage_dir" \
    "-DCMAKE_PREFIX_PATH=$sdk_dir;/mingw64" \
    "-DQca3-qt5_DIR=$sdk_dir/lib/cmake/Qca3-qt5" \
    "-DQt5Keychain_DIR=$sdk_dir/lib/cmake/Qt5Keychain" \
    "-DTRANSLATIONS_DIR=$l10n_dir/translations" \
    "-DCMAKE_C_FLAGS=-D_WIN32_WINNT=$winver -DWINVER=$winver" \
    "-DCMAKE_CXX_FLAGS=-D_WIN32_WINNT=$winver -DWINVER=$winver" \
    -DQT_DEFAULT_MAJOR_VERSION=5 \
    -DUSE_QT6=OFF \
    -DUSE_MXE=OFF \
    -DUSE_CCACHE=ON \
    -DCHAT_TYPE=WEBKIT \
    -DBUNDLED_IRIS=ON \
    -DBUNDLED_IRIS_ALL=OFF \
    -DIRIS_BUNDLED_QCA=OFF \
    -DIRIS_SYSTEM_QCA=3 \
    -DIRIS_BUNDLED_USRSCTP=OFF \
    -DIRIS_BUNDLED_OMEMO_C=OFF \
    -DENABLE_OMEMO=ON \
    -DUSRSCTP_INCLUDE_DIR=/mingw64/include \
    -DUSRSCTP_LIB_DIR=/mingw64/lib \
    -DHUNSPELL_INCLUDE_DIR=/mingw64/include/hunspell \
    -DHUNSPELL_LIBRARY=/mingw64/lib/libhunspell.dll.a \
    -DUSE_HUNSPELL=ON \
    -DUSE_KEYCHAIN=ON \
    -DBUILD_TESTING=OFF \
    -DENABLE_PLUGINS=OFF \
    -DBUILD_PSIMEDIA=OFF \
    -DONLY_BINARY=OFF \
    -DINSTALL_EXTRA_FILES=ON \
    2>&1 | tee "$output_dir/configure-$profile.log"

configure_log="$output_dir/configure-$profile.log"
grep -F 'Chatlog type - QtWebKit' "$configure_log"
grep -F 'QCA: selected system QCA 3' "$configure_log"
grep -F 'Found UsrSCTP' "$configure_log"
grep -Eq 'Found (PkgConfig::)?OmemoC|libomemo-c' "$configure_log"
grep -Eq '^-- Git Version [0-9]' "$configure_log"
! grep -F 'Git Version windows-sdk-' "$configure_log"
grep -Eq '^BUNDLED_IRIS:BOOL=ON$' "$build_dir/CMakeCache.txt"
grep -Eq '^IRIS_BUNDLED_QCA:BOOL=OFF$' "$build_dir/CMakeCache.txt"
grep -Eq '^IRIS_BUNDLED_USRSCTP:BOOL=OFF$' "$build_dir/CMakeCache.txt"
grep -Eq '^IRIS_BUNDLED_OMEMO_C:BOOL=OFF$' "$build_dir/CMakeCache.txt"
grep -Eq '^MINIZIP_LIBRARY:FILEPATH=.*libminizip' "$build_dir/CMakeCache.txt"
grep -F 'Found ccache at ' "$configure_log"

cmake --build "$build_dir" --parallel 4
ccache --show-stats
cmake --install "$build_dir"
test -f "$stage_dir/psi.exe"
test -n "$(find "$stage_dir/translations" -maxdepth 1 -type f -name 'psi_*.qm' -print -quit)"

# Keep the portable tree useful on its own. The NSIS installer consumes exactly
# the same tree, so these files are automatically present there as well.
cp "$source_dir/COPYING" "$stage_dir/COPYING"
cp "$source_dir/README.html" "$stage_dir/README.html"

# MSYS2 suffixes Qt5 host tools to allow Qt5 and Qt6 to coexist. The packaging
# helper also supports the upstream unsuffixed name; provide that alias for the
# current MSYS2 Qt5 package without maintaining another Qt deployment path.
if [[ ! -f /mingw64/bin/windeployqt.exe ]]; then
    if [[ -f /mingw64/bin/windeployqt-qt5.exe ]]; then
        cp /mingw64/bin/windeployqt-qt5.exe /mingw64/bin/windeployqt.exe
    else
        echo "Neither windeployqt.exe nor windeployqt-qt5.exe was found" >&2
        exit 1
    fi
fi

python packaging/windows/prepare_package.py \
    --profile "$profile" \
    --root "$stage_dir" \
    --sdk "$sdk_dir" \
    --mingw-prefix /mingw64 \
    --source-root "$source_dir" \
    --qt-conf "$source_dir/win32/qt.conf" \
    --report "$report"

app_version=$(python - "$build_dir/src/psi_win.rc" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
match = re.search(r'VALUE "ProductVersion", "([^"\\]+)\\0"', text)
if not match:
    raise SystemExit("Could not read ProductVersion from generated psi_win.rc")
print(match.group(1))
PY
)

numeric_version=$(python - "$app_version" <<'PY'
import re
import sys

parts = sys.argv[1].split(".")
if not parts or any(not re.fullmatch(r"\d+", p) for p in parts):
    raise SystemExit(f"NSIS requires a numeric application version, got: {sys.argv[1]}")
parts = (parts + ["0"] * 4)[:4]
if any(int(p) > 65535 for p in parts):
    raise SystemExit(f"NSIS version component exceeds 65535: {sys.argv[1]}")
print(".".join(parts))
PY
)

psi_revision=$(git rev-parse HEAD)
cat > "$stage_dir/BUILD-INFO.txt" <<EOF
Psi Windows package
===================
Psi version: $app_version
Psi revision: $psi_revision
Windows profile: $profile
Compiler baseline: _WIN32_WINNT=$winver, WINVER=$winver
Translations: psi-im/psi-l10n $l10n_revision
EOF

package_base="psi-${app_version}-${package_os}-x64"
zip_file="$output_dir/${package_base}.zip"
setup_file="$output_dir/${package_base}-setup.exe"
manifest="$output_dir/uninstall-$profile.nsh"

python packaging/windows/generate_nsis_manifest.py \
    --root "$stage_dir" \
    --output "$manifest"

needs_vcredist=$(python - "$report" <<'PY'
import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
try:
    start = lines.index("MSVC runtime imports:") + 1
except ValueError:
    raise SystemExit("dependency report has no MSVC runtime section")
values = []
for line in lines[start:]:
    if not line.strip():
        break
    values.append(line.strip())
print("true" if values and values != ["none"] else "false")
PY
)

nsis_vcredist=()
if [[ "$needs_vcredist" == true ]]; then
    redist="$output_dir/vc_redist-$profile.x64.exe"
    if [[ "$profile" == modern ]]; then
        redist_url='https://aka.ms/vc14/vc_redist.x64.exe'
    else
        redist_url='https://aka.ms/vs/17/release/vc_redist.x64.exe'
    fi
    echo "Downloading Microsoft Visual C++ Redistributable: $redist_url"
    curl --fail --location --retry 3 --output "$redist" "$redist_url"
    test -s "$redist"
    nsis_vcredist=(-DVC_REDIST="$(cygpath -aw "$redist")")
fi

rm -f "$zip_file" "$setup_file"
(
    cd "$stage_dir"
    zip -9 -r "$zip_file" .
)
unzip -t "$zip_file"

makensis \
    "${nsis_profile[@]}" \
    "${nsis_vcredist[@]}" \
    -DAPP_VERSION="$app_version" \
    -DAPP_VERSION_NUMERIC="$numeric_version" \
    -DSOURCE_DIR="$(cygpath -aw "$stage_dir")" \
    -DUNINSTALL_MANIFEST="$(cygpath -aw "$manifest")" \
    -DOUTPUT_FILE="$(cygpath -aw "$setup_file")" \
    -DAPP_ICON="$(cygpath -aw "$source_dir/win32/app.ico")" \
    -DLICENSE_FILE="$(cygpath -aw "$source_dir/COPYING")" \
    packaging/windows/psi.nsi

test -s "$setup_file"
echo "$app_version" > "$output_dir/version-$profile.txt"
echo "Created $zip_file"
echo "Created $setup_file"