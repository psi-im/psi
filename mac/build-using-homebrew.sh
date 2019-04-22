#!/bin/sh

# Authors: Boris Pek
# License: Public Domain
# Created: 2018-10-07
# Updated: 2019-04-23
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
# Build dependencies of Psi and useful tools:
# brew install --build-bottle pkg-config htop cmake coreutils gettext
# brew install --build-bottle openssl pcre pcre2 libunistring libidn libidn2
# brew install --build-bottle qt qtkeychain qca
# brew install --build-bottle autoconf automake libtool readline
# brew install --build-bottle minizip hunspell
#
# Additional tools:
# brew install --build-bottle curl wget git
#
# Build dependencies of Psi plugins:
# brew install --build-bottle tidy-html5 libgpg-error libgcrypt libotr
# brew install --build-bottle libsignal-protocol-c
#
# Enabling of qca-gnupg plugin if necessary:
# Apply changes from: https://github.com/tehnick/homebrew-core/commit/2f866aee
# Then:
# brew uninstall qca
# brew install --build-bottle gmp libtasn1 nettle libffi p11-kit libevent unbound
# brew install --build-bottle adns libassuan libksba libusb npth pinentry gnutls
# brew install --build-bottle gnupg qca
#
# If you were uncareful and have installed Homebrew to /usr/local as suggested
# by its developers now, then export HOMEBREW environment variable before
# launching this script or uncomment next string:
# export HOMEBREW="/usr/local"

set -e

PATH="${HOMEBREW}/bin:${PATH}"
CUR_DIR="$(dirname $(realpath -s ${0}))"
MAIN_DIR="$(realpath -s ${CUR_DIR}/..)"
TOOLCHAIN_FILE="${CUR_DIR}/homebrew-toolchain.cmake"

[ -d "${MAIN_DIR}/src/plugins/generic" ] && \
    ENABLE_PLUGINS="ON" || \
    ENABLE_PLUGINS="OFF"

[ "${ENABLE_WEBENGINE}" = "ON" ] && \
     CHAT_TYPE="webengine" || \
     CHAT_TYPE="basic"

BUILD_OPTIONS="-DCMAKE_BUILD_TYPE=Release \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DCHAT_TYPE=${CHAT_TYPE} \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DUSE_SPARKLE=OFF \
               -DUSE_QJDNS=OFF \
               -DUSE_CCACHE=OFF \
               -DBUILD_DEV_PLUGINS=OFF \
               -DVERBOSE_PROGRAM_NAME=ON"

mkdir -p "${MAIN_DIR}/builddir"
cd "${MAIN_DIR}/builddir"

cmake .. -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" ${BUILD_OPTIONS} ${@}
cmake --build . --target all -- -j4

[ "${BUILD_ONLY}" = "true" ] && exit 0

cpack -G DragNDrop
cp -a Psi*.dmg "${MAIN_DIR}/../"
echo

echo "App bundle is successfully built!"
echo

