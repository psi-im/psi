#!/bin/sh

set -e

if [ -z "${1}" ]; then
    echo "Error! The name of release branch is not specified!"
    echo "Example of usage:"
    echo "${0} release-1.x"
    exit 1
fi

MAIN_DIR="$(realpath -s $(dirname ${0})/..)"
cd "${MAIN_DIR}"

git checkout master
git merge "origin/${1}" --no-ff --no-commit 2>&1 > /dev/null || true
git checkout master . 2>&1 > /dev/null
git commit -a -m "Merge remote-tracking branch 'origin/${1}'"

