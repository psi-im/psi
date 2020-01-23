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

[ -z "${PSI_PLUS}" ] && PSI_PLUS="OFF"

[ -d "./src/plugins/generic" ] && \
    ENABLE_PLUGINS="ON" || \
    ENABLE_PLUGINS="OFF"

[ "${ENABLE_WEBKIT}" = "OFF" ] && \
    CHAT_TYPE="basic" || \
    CHAT_TYPE="webkit"

BUILD_OPTIONS="-DCMAKE_INSTALL_PREFIX=/usr \
               -DCMAKE_BUILD_TYPE=Release \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DCHAT_TYPE=${CHAT_TYPE} \
               -DPSI_PLUS=${PSI_PLUS} \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DUSE_SPARKLE=OFF \
               -DUSE_QJDNS=OFF \
               -DBUILD_DEV_PLUGINS=OFF \
               -DVERBOSE_PROGRAM_NAME=ON \
               "

mkdir -p builddir
cd builddir

cmake .. ${BUILD_OPTIONS} \
      -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
      -DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
      -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}"
make -k -j $(nproc) VERBOSE=1
sudo make install -j 1

