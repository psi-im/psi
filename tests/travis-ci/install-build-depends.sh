#!/bin/sh

# Author:  Boris Pek
# Version: N/A
# License: Public Domain

set -e
set -x

if [ "${TARGET}" = "linux64" ]
then
    # Add official Psi+ PPA with extra packages:
    sudo add-apt-repository ppa:psi-plus/ppa -ysu

    sudo apt-get update  -qq
    sudo apt-get install -qq cmake \
                             libhunspell-dev \
                             libidn11-dev \
                             libminizip-dev \
                             libotr5-dev \
                             libqca-qt5-2-dev \
                             libqt5svg5-dev \
                             libqt5webkit5-dev \
                             libqt5x11extras5-dev \
                             libsignal-protocol-c-dev \
                             libsm-dev \
                             libssl-dev \
                             libtidy-dev \
                             libxss-dev \
                             qt5keychain-dev \
                             qtmultimedia5-dev \
                             zlib1g-dev
fi

if [ "${TARGET}" = "macos64" ]
then
    HOMEBREW_NO_AUTO_UPDATE=1
    PACKAGES="ccache \
              libidn \
              qtkeychain \
              qca \
              minizip \
              hunspell \
              tidy-html5 \
              libgpg-error \
              libotr \
              libsignal-protocol-c"
    for PKG in ${PACKAGES}
    do
        brew ls --versions "${PKG}" > /dev/null || brew install "${PKG}"
    done
fi

