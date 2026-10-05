#!/usr/bin/env bash
set -euo pipefail

iris_dir=${1:-iris}

if [[ ! -e "$iris_dir/.git" && ! -d "$iris_dir/.git" ]]; then
    echo "Iris submodule is not initialized: $iris_dir" >&2
    exit 1
fi

if [[ "$(git -C "$iris_dir" rev-parse --is-shallow-repository)" == "true" ]]; then
    git -C "$iris_dir" fetch --unshallow --tags origin
else
    git -C "$iris_dir" fetch --tags origin
fi

version_tag=$(git -C "$iris_dir" describe --tags --abbrev=0 --match 'v[0-9]*' HEAD 2>/dev/null || true)
if [[ ! "$version_tag" =~ ^v[0-9]+\.[0-9]+(\.[0-9]+)?(\.[0-9]+)?$ ]]; then
    echo "No valid Iris release tag is reachable from $(git -C "$iris_dir" rev-parse HEAD)" >&2
    exit 1
fi

echo "Iris version tag: $version_tag"
