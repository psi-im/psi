#!/bin/sh

cd $(dirname "$0")

if [ "$(git describe --tags | grep '\-' | wc -l)" = "1" ]; then
    git describe --tags | cut -d - -f 2
else
    echo 0
fi
