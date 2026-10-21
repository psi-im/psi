#!/bin/sh

# Authors: Boris Pek
# License: Public Domain
# Created: 2018-10-07
# Updated: 2024-05-17
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
# brew install hunspell minizip qt@5 qtkeychain
#
# Build dependencies for Psi plugins:
# brew install tidy-html5 libotr libomemo-c
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

[ -z "${CHAT_TYPE}" ] && \
    CHAT_TYPE="basic"

[ "${ENABLE_DEV_PLUGINS}" != "ON" ] && \
    ENABLE_DEV_PLUGINS="OFF"

[ "${ENABLE_PSIMEDIA}" != "ON" ] && \
    ENABLE_PSIMEDIA="OFF"

# Opt in to a Qt6.12 release build with PSI_QT6_RELEASE=ON.
# Unlike the legacy Homebrew/Qt5 path, this requires an explicitly installed,
# version-checked Qt SDK. It may be provisioned by install-qt-action on macOS.
if [ "${PSI_QT6_RELEASE:-OFF}" = "ON" ]; then
    EXPECTED_QT="$(python3 -c 'import json;print(json.load(open("dependencies.lock.json"))["qt"]["qt6"]["version"])')"
    if [ -z "${QT_ROOT_DIR:-}" ] || [ ! -x "${QT_ROOT_DIR}/bin/qmake" ]; then
        echo "Qt6 release requires QT_ROOT_DIR pointing to official Qt ${EXPECTED_QT}" >&2
        exit 1
    fi
    ACTUAL_QT="$("${QT_ROOT_DIR}/bin/qmake" -query QT_VERSION)"
    if [ "${ACTUAL_QT}" != "${EXPECTED_QT}" ]; then
        echo "Expected Qt ${EXPECTED_QT}, found ${ACTUAL_QT}" >&2
        exit 1
    fi
    QT6_OPTIONS="-DQT_DEFAULT_MAJOR_VERSION=6 -DUSE_QT6=ON -DCMAKE_PREFIX_PATH=${QT_ROOT_DIR}"
else
    QT6_OPTIONS=""
fi

BUILD_OPTIONS="-DCMAKE_BUILD_TYPE=Release \
               -DCHAT_TYPE=${CHAT_TYPE} \
               -DBUILD_DEV_PLUGINS=${ENABLE_DEV_PLUGINS} \
               -DBUILD_PSIMEDIA=${ENABLE_PSIMEDIA} \
               -DENABLE_PLUGINS=${ENABLE_PLUGINS} \
               -DUSE_SPARKLE=OFF \
               -DUSE_HUNSPELL=ON \
               -DUSE_KEYCHAIN=ON \
               -DBUNDLED_KEYCHAIN=ON \
               -DBUNDLED_OMEMO_C_ALL=ON \
               -DIRIS_BUNDLED_QCA=ON \
               -DIRIS_BUNDLED_USRSCTP=ON \
               -DVERBOSE_PROGRAM_NAME=ON \
               ${QT6_OPTIONS}"

mkdir -p "${MAIN_DIR}/builddir"
cd "${MAIN_DIR}/builddir"

which nproc > /dev/null && JOBS=$(nproc) || JOBS=4

cmake .. -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" ${BUILD_OPTIONS} ${@}
cmake --build . --target all -- -j ${JOBS}

[ "${BUILD_ONLY}" = "true" ] && exit 0

cpack -G DragNDrop
cp -a Psi*.dmg "${MAIN_DIR}/../"
echo

echo "App bundle is built successfully!"
echo
