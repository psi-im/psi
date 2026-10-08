#!/usr/bin/env bash
set -euo pipefail

if [[ ! -e iris/.git ]]; then
    git submodule update --init --depth 1 iris
fi

if [[ "$(git -C iris rev-parse --is-shallow-repository)" == "true" ]]; then
    git -C iris fetch --unshallow --tags origin
else
    git -C iris fetch --tags origin
fi

git -C iris describe --tags --abbrev=0 --match 'v[0-9]*' HEAD >/dev/null
