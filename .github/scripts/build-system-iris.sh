#!/usr/bin/env bash
set -euo pipefail

: "${IRIS_INSTALL_PREFIX:?IRIS_INSTALL_PREFIX must be set}"
: "${IRIS_C_COMPILER:?IRIS_C_COMPILER must be set}"
: "${IRIS_CXX_COMPILER:?IRIS_CXX_COMPILER must be set}"

iris_ref=$(git ls-tree HEAD iris | awk '{print $3}')
if [[ ! "$iris_ref" =~ ^[0-9a-f]{40}$ ]]; then
    echo "::error::Cannot resolve the Iris gitlink from Psi HEAD"
    exit 1
fi

# Check the SDK coordinate before deleting work directories or downloading any
# package. A release tag must never substitute another commit for the gitlink.
dependency_values=$(GITHUB_ENV= GITHUB_OUTPUT= python3 .github/scripts/resolve-dependencies.py --verify-iris-release)
iris_release_tag=$(awk -F= '$1 == "IRIS_RELEASE_TAG" { print $2 }' <<< "$dependency_values")

work_root="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
iris_src="$work_root/psi-system-iris-src"
iris_build="$work_root/psi-system-iris-build"
iris_release="$work_root/psi-system-iris-release"
iris_probe="$work_root/psi-system-iris-probe"
rm -rf "$iris_src" "$iris_build" "$iris_release" "$iris_probe" "$IRIS_INSTALL_PREFIX"

probe_installed_iris() {
    rm -rf "$iris_probe"
    mkdir -p "$iris_probe/src"
    cat > "$iris_probe/src/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.16)
project(psi_iris_release_probe LANGUAGES CXX)
find_package(Iris CONFIG REQUIRED)
if(NOT Iris_VERSION VERSION_EQUAL PSI_EXPECTED_IRIS_VERSION)
    message(FATAL_ERROR "Installed Iris SDK version does not match the locked release")
endif()
if(NOT Iris_QT_MAJOR_VERSION EQUAL PSI_EXPECTED_QT_MAJOR)
    message(FATAL_ERROR "Installed Iris SDK Qt major version does not match Psi")
endif()
add_executable(psi-iris-release-probe main.cpp)
target_link_libraries(psi-iris-release-probe PRIVATE Iris::Iris)
CMAKE
    cat > "$iris_probe/src/main.cpp" <<'CPP'
#include <iris/jid/jid.h>

int main()
{
    return XMPP::Jid(QStringLiteral("ci@example.org")).isValid() ? 0 : 1;
}
CPP

    cmake -S "$iris_probe/src" -B "$iris_probe/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$IRIS_C_COMPILER" \
        -DCMAKE_CXX_COMPILER="$IRIS_CXX_COMPILER" \
        -DPSI_EXPECTED_IRIS_VERSION="${iris_release_tag#v}" \
        -DPSI_EXPECTED_QT_MAJOR=5 \
        -DCMAKE_PREFIX_PATH="$IRIS_INSTALL_PREFIX" >/dev/null \
        && cmake --build "$iris_probe/build" --parallel >/dev/null
}

try_release_package() {
    local tag asset archive extract_root

    tag=$iris_release_tag

    if [[ ! -r /etc/os-release ]]; then
        echo "Cannot identify Linux distribution; building Iris $tag from source"
        return 1
    fi

    # shellcheck disable=SC1091
    source /etc/os-release
    if [[ "${ID:-}" != "ubuntu" || -z "${VERSION_ID:-}" ]]; then
        echo "No Iris release package mapping for ${ID:-unknown} ${VERSION_ID:-unknown}; building $tag from source"
        return 1
    fi

    asset="iris-deb-ubuntu-${VERSION_ID}.zip"
    archive="$iris_release/$asset"
    extract_root="$iris_release/root"
    mkdir -p "$iris_release/archive" "$extract_root" "$IRIS_INSTALL_PREFIX"

    echo "Iris gitlink $iris_ref is tagged $tag; trying release asset $asset"
    if ! curl --fail --location --silent --show-error --retry 3 \
        "https://github.com/psi-im/iris/releases/download/$tag/$asset" \
        --output "$archive"; then
        echo "Release asset $asset is unavailable; building Iris $tag from source"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    if ! (cd "$iris_release/archive" && cmake -E tar xvf "$archive" >/dev/null); then
        echo "Cannot unpack $asset; building Iris $tag from source"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    mapfile -t debs < <(find "$iris_release/archive" -type f -name '*.deb' -print | sort)
    if (( ${#debs[@]} == 0 )) || ! command -v dpkg-deb >/dev/null 2>&1; then
        echo "Release asset does not contain usable Debian packages; building Iris $tag from source"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    for deb in "${debs[@]}"; do
        dpkg-deb -x "$deb" "$extract_root"
    done
    if [[ ! -d "$extract_root/usr" ]]; then
        echo "Release packages do not contain a /usr payload; building Iris $tag from source"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi
    cp -a "$extract_root/usr/." "$IRIS_INSTALL_PREFIX/"

    if ! probe_installed_iris; then
        echo "Release SDK is incompatible with this Linux/Qt environment; building Iris $tag from source"
        rm -rf "$iris_release" "$iris_probe" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    rm -rf "$iris_release" "$iris_probe"
    echo "Using Iris $tag release package for gitlink $iris_ref"
    return 0
}

if ! try_release_package; then
    git init "$iris_src"
    git -C "$iris_src" remote add origin https://github.com/psi-im/iris.git
    git -C "$iris_src" fetch --filter=blob:none --tags origin "$iris_ref"
    git -C "$iris_src" checkout --detach "$iris_ref"

    cmake -S "$iris_src" -B "$iris_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$IRIS_C_COMPILER" \
        -DCMAKE_CXX_COMPILER="$IRIS_CXX_COMPILER" \
        -DCMAKE_INSTALL_PREFIX="$IRIS_INSTALL_PREFIX" \
        -DBUILD_SHARED_LIBS=ON \
        -DIRIS_BUILD_TESTS=OFF \
        -DIRIS_ENABLE_INSTALL=ON \
        -DIRIS_SYSTEM_QCA=2 \
        -DIRIS_BUNDLED_QCA=OFF \
        -DIRIS_ENABLE_OMEMO=ON \
        -DIRIS_BUNDLED_OMEMO_C=OFF \
        -DIRIS_BUNDLED_USRSCTP=OFF
    cmake --build "$iris_build" --parallel
    cmake --install "$iris_build"

    rm -rf "$iris_src" "$iris_build"
    echo "Built external Iris $iris_ref into $IRIS_INSTALL_PREFIX"
fi

test ! -e iris/CMakeLists.txt
if ! find "$IRIS_INSTALL_PREFIX" -type f -path '*/cmake/Iris/IrisConfig.cmake' -print -quit | grep -q .; then
    echo "::error::Installed Iris CMake package was not found under $IRIS_INSTALL_PREFIX"
    exit 1
fi
