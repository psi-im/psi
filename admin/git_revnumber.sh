#!/bin/sh

cd $(dirname "$0")

ref_commit="$(git describe --tags | cut -d - -f1)"

if [ ! -z "${1}" ]; then
    if [ "$(git tag | grep -x "^${1}$" | wc -l)" = "1" ]; then
        ref_commit="${1}"
    fi
fi

git rev-list --count ${ref_commit}..HEAD

