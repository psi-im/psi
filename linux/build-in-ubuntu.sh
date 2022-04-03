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

[ -d "./plugins/generic" ] && \
    ENABLE_PLUGINS="ON" || \
    ENABLE_PLUGINS="OFF"

[ -z "${CHAT_TYPE}" ] && \
    CHAT_TYPE="basic"

[ "${ENABLE_DEV_PLUGINS}" != "ON" ] && \
    ENABLE_DEV_PLUGINS="OFF"

[ "${ENABLE_PSIMEDIA}" != "ON" ] && \
    ENABLE_PSIMEDIA="OFF"

BUILD_OPTIONS="-DCMAKE_INSTALL_PREFIX=/usr \
               -DCMAKE_BUILD_TYPE=Release \
               -DCHAT_TYPE=${CHAT_TYPE} \
               -DBUILD_DEV_PLUGINS=${ENABLE_DEV_PLUGINS} \
               -DBUILD_PSIMEDIA=${ENABLE_PSIMEDIA} \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DUSE_SPARKLE=OFF \
               -DBUNDLED_QCA=ON \
               -DBUNDLED_USRSCTP=ON \
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
