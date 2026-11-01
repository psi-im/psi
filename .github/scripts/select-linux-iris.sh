#!/usr/bin/env bash
set -euo pipefail

: "${IRIS_INSTALL_PREFIX:?IRIS_INSTALL_PREFIX must be set}"
: "${IRIS_C_COMPILER:?IRIS_C_COMPILER must be set}"
: "${IRIS_CXX_COMPILER:?IRIS_CXX_COMPILER must be set}"
: "${IRIS_QT_MAJOR:=5}"

iris_ref=$(git ls-tree HEAD iris | awk '{print $3}')
if [[ ! "$iris_ref" =~ ^[0-9a-f]{40}$ ]]; then
    echo "::error::Cannot resolve the Iris gitlink from Psi HEAD"
    exit 1
fi

# Only a package released for the exact gitlink and this Qt/platform can be
# reused. Otherwise Psi itself builds its initialized Iris git submodule.
dependency_values=$(GITHUB_ENV= GITHUB_OUTPUT= python3 .github/scripts/resolve-dependencies.py --resolve-iris-release)
iris_release_tag=$(awk -F= '$1 == "IRIS_RELEASE_TAG" { print $2 }' <<< "$dependency_values")

work_root="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
iris_release="$work_root/psi-system-iris-release"
iris_probe="$work_root/psi-system-iris-probe"
rm -rf "$iris_release" "$iris_probe" "$IRIS_INSTALL_PREFIX"

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
        -DPSI_EXPECTED_QT_MAJOR="$IRIS_QT_MAJOR" \
        -DCMAKE_PREFIX_PATH="$IRIS_INSTALL_PREFIX" >/dev/null \
        && cmake --build "$iris_probe/build" --parallel >/dev/null
}

try_release_package() {
    local tag asset archive extract_root

    tag=$iris_release_tag
    if [[ -z "$tag" ]]; then
        echo "Iris gitlink has no released SDK; using Psi's pinned Iris submodule"
        return 1
    fi

    if [[ ! -r /etc/os-release ]]; then
        echo "Cannot identify Linux distribution; using Psi's pinned Iris submodule"
        return 1
    fi

    # shellcheck disable=SC1091
    source /etc/os-release
    if [[ "${ID:-}" != "ubuntu" || -z "${VERSION_ID:-}" ]]; then
        echo "No Iris release package mapping for ${ID:-unknown} ${VERSION_ID:-unknown}; using Psi's pinned Iris submodule"
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
        echo "Release asset $asset is unavailable; using Psi's pinned Iris submodule"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    if ! (cd "$iris_release/archive" && cmake -E tar xvf "$archive" >/dev/null); then
        echo "Cannot unpack $asset; using Psi's pinned Iris submodule"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    mapfile -t debs < <(find "$iris_release/archive" -type f -name '*.deb' -print | sort)
    if (( ${#debs[@]} == 0 )) || ! command -v dpkg-deb >/dev/null 2>&1; then
        echo "Release asset does not contain usable Debian packages; using Psi's pinned Iris submodule"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    for deb in "${debs[@]}"; do
        dpkg-deb -x "$deb" "$extract_root"
    done
    if [[ ! -d "$extract_root/usr" ]]; then
        echo "Release packages do not contain a /usr payload; using Psi's pinned Iris submodule"
        rm -rf "$iris_release" "$IRIS_INSTALL_PREFIX"
        return 1
    fi
    cp -a "$extract_root/usr/." "$IRIS_INSTALL_PREFIX/"

    if ! probe_installed_iris; then
        echo "Release SDK is incompatible with this Linux/Qt environment; using Psi's pinned Iris submodule"
        rm -rf "$iris_release" "$iris_probe" "$IRIS_INSTALL_PREFIX"
        return 1
    fi

    rm -rf "$iris_release" "$iris_probe"
    echo "Using Iris $tag release package for gitlink $iris_ref"
    return 0
}

# Never compile/install Iris a second time in a separate SDK tree. The
# fallback is the normal add_subdirectory(iris) inside Psi's CMake build.
if try_release_package; then
    iris_bundled=OFF
    iris_prefix=$IRIS_INSTALL_PREFIX
else
    iris_bundled=ON
    iris_prefix=
    rm -rf "$iris_release" "$iris_probe" "$IRIS_INSTALL_PREFIX"
fi

printf 'Iris selection: BUNDLED_IRIS=%s tag=%s gitlink=%s prefix=%s\n' \
    "$iris_bundled" "${iris_release_tag:-unreleased}" "$iris_ref" "$iris_prefix"
if [[ -n "${GITHUB_ENV:-}" ]]; then
    printf 'IRIS_BUNDLED=%s\nIRIS_PREFIX=%s\n' "$iris_bundled" "$iris_prefix" >> "$GITHUB_ENV"
fi
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    printf 'iris_bundled=%s\niris_prefix=%s\n' "$iris_bundled" "$iris_prefix" >> "$GITHUB_OUTPUT"
fi
