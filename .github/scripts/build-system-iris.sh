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

iris_src="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/psi-system-iris-src"
iris_build="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/psi-system-iris-build"
rm -rf "$iris_src" "$iris_build" "$IRIS_INSTALL_PREFIX"

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

test ! -e iris/CMakeLists.txt
if ! find "$IRIS_INSTALL_PREFIX" -type f -path '*/cmake/Iris/IrisConfig.cmake' -print -quit | grep -q .; then
    echo "::error::Installed Iris CMake package was not found under $IRIS_INSTALL_PREFIX"
    exit 1
fi

echo "Built external Iris $iris_ref into $IRIS_INSTALL_PREFIX"
