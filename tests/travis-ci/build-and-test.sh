#!/bin/sh

# Author:  Boris Pek
# Version: N/A
# License: Public Domain

set -e
set -x

if [ "${TARGET}" = "linux64" ]
then
    ./tests/travis-ci/build-in-ubuntu.sh

    ls -alp /usr/bin/psi*
    ls -alp /usr/share/applications/psi*
    ls -alp /usr/share/pixmaps/psi*
    ls -alp /usr/share/psi*

    du -shc /usr/bin/psi*
    du -shc /usr/share/applications/psi*
    du -shc /usr/share/pixmaps/psi*
    du -shc /usr/share/psi*

    if [ -d "./plugins/generic" ]
    then
        ls -alp /usr/lib/psi*/plugins/*
        du -shc /usr/lib/psi*/plugins/*
    fi
fi

if [ "${TARGET}" = "macos64" ]
then
    ./mac/build-using-homebrew.sh

    ls -alp ../Psi*.dmg
    du -shc ../Psi*.dmg
fi

