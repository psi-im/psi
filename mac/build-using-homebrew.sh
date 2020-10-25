#!/bin/sh

# Authors: Boris Pek
# License: Public Domain
# Created: 2018-10-07
# Updated: 2020-10-26
# Version: N/A
#
# Description: script for building of app bundles for macOS
#
# This script should be as simple as possible: it builds Psi IM or Psi+ as is,
# without downloading of extra resources.
#
# If you are interested in the most complete builds (which includes all Psi
# plugins, all available translations, extra iconsets, sounds, skins, themes,
# recent spell-checking dictionaries from LibreOffice project, etc.) see:
# https://github.com/psi-plus/maintenance/tree/master/scripts/macosx/tehnick
#
# Ready to use app bundles built by that scripts are hosted on SourceForge:
# https://sourceforge.net/projects/psiplus/files/macOS/tehnick/
# https://sourceforge.net/projects/psi/files/Experimental-Builds/macOS/tehnick/
#
# Build dependencies for Psi and useful tools:
# export HOMEBREW_NO_BOTTLE_SOURCE_FALLBACK=1
# brew install ccache coreutils cmake
# brew install openssl@1.1 libusrsctp minizip hunspell qt qtkeychain
#
# Build qca with enabled qca-gnupg plugin:
# brew install --build-bottle https://raw.githubusercontent.com/tehnick/homebrew-core/hobby/Formula/qca.rb
# end
#
# Build dependencies for Psi plugins:
# brew install tidy-html5 libotr libsignal-protocol-c
#
# Additional tools:
# brew install gnupg pinentry-mac wget htop

set -e

[ -z "${HOMEBREW}" ] && HOMEBREW="/usr/local"

PATH="${HOMEBREW}/bin:${PATH}"
PATH="${HOMEBREW}/opt/ccache/libexec:${PATH}"
CUR_DIR="$(dirname $(realpath -s ${0}))"
MAIN_DIR="$(realpath -s ${CUR_DIR}/..)"
TOOLCHAIN_FILE="${CUR_DIR}/homebrew-toolchain.cmake"

[ -d "${MAIN_DIR}/plugins/generic" ] && \
    ENABLE_PLUGINS="ON" || \
    ENABLE_PLUGINS="OFF"

[ "${ENABLE_WEBENGINE}" = "ON" ] && \
    CHAT_TYPE="webengine" || \
    CHAT_TYPE="basic"

[ "${ENABLE_DEV_PLUGINS}" != "ON" ] && \
    ENABLE_DEV_PLUGINS="OFF"

[ "${ENABLE_PSIMEDIA}" != "ON" ] && \
    ENABLE_PSIMEDIA="OFF"

BUILD_OPTIONS="-DCMAKE_BUILD_TYPE=Release \
               -DCHAT_TYPE=${CHAT_TYPE} \
               -DBUILD_DEV_PLUGINS=${ENABLE_DEV_PLUGINS} \
               -DBUILD_PSIMEDIA=${ENABLE_PSIMEDIA} \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DUSE_SPARKLE=OFF \
               -DUSE_QJDNS=OFF \
               -DVERBOSE_PROGRAM_NAME=ON"

mkdir -p "${MAIN_DIR}/builddir"
cd "${MAIN_DIR}/builddir"

which nproc > /dev/null && JOBS=$(nproc) || JOBS=4

cmake .. -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" ${BUILD_OPTIONS} ${@}
cmake --build . --target all -- -j ${JOBS}

[ "${BUILD_ONLY}" = "true" ] && exit 0

cpack -G DragNDrop
cp -a Psi*.dmg "${MAIN_DIR}/../"
echo

echo "App bundle is successfully built!"
echo
