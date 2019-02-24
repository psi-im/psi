#!/bin/sh

# Author:  Boris Pek
# Version: N/A
# License: Public Domain

set -e
set -x

EXTRA_FLAGS="${EXTRA_FLAGS} -Wall"
CPPFLAGS="$(dpkg-buildflags --get CPPFLAGS) ${EXTRA_FLAGS}"
CFLAGS="$(dpkg-buildflags --get CFLAGS) ${CPPFLAGS}"
CXXFLAGS="$(dpkg-buildflags --get CXXFLAGS) ${CPPFLAGS}"
LDFLAGS="$(dpkg-buildflags --get LDFLAGS) -Wl,--as-needed"

[ -d "./src/plugins/generic" ] && \
    ENABLE_PLUGINS="ON" || \
    ENABLE_PLUGINS="OFF"

[ "${WITHOUT_WEBKIT}" = "true" ] && \
    ENABLE_WEBKIT="OFF" || \
    ENABLE_WEBKIT="ON"

BUILD_OPTIONS="-DCMAKE_INSTALL_PREFIX=/usr \
               -DCMAKE_BUILD_TYPE=Release \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DENABLE_WEBKIT=${ENABLE_WEBKIT} \
               -DUSE_WEBKIT=${ENABLE_WEBKIT} \
               -DUSE_WEBENGINE=OFF \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DUSE_SPARKLE=OFF \
               -DUSE_QJDNS=OFF \
               -DUSE_CCACHE=OFF \
               -DBUILD_DEV_PLUGINS=OFF \
               -DVERBOSE_PROGRAM_NAME=ON \
               .."

mkdir -p builddir
cd builddir

cmake ${BUILD_OPTIONS} \
      -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
      -DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
      -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}"
make VERBOSE=1 -k -j $(nproc)
sudo make install -j 1

