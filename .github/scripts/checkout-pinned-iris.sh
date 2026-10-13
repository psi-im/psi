#!/usr/bin/env bash
# Fetch the exact Iris gitlink. GitHub may reject a direct shallow fetch by
# SHA even when that object is advertised at refs/heads/master.
set -euo pipefail

expected="$(git rev-parse HEAD:iris)"
if [[ ! "$expected" =~ ^[0-9a-f]{40}$ ]]; then
    echo "::error::Invalid Iris gitlink: $expected" >&2
    exit 1
fi

if ! git submodule update --init --depth 1 iris; then
    echo "::warning::Direct shallow Iris gitlink fetch failed; fetching advertised branch refs"
    # The first attempt initialized .git/modules/iris. Fetch by advertised
    # branch ref instead of requesting an unadvertised SHA via upload-pack.
    git -C iris fetch --no-tags origin '+refs/heads/master:refs/remotes/origin/master'
    if ! git -C iris cat-file -e "${expected}^{commit}" 2>/dev/null; then
        # The gitlink may intentionally lag Iris's current master.
        git -C iris fetch --tags origin '+refs/heads/master:refs/remotes/origin/master'
    fi
    git -C iris cat-file -e "${expected}^{commit}" || {
        echo "::error::Pinned Iris commit $expected is unavailable even after fetching master history" >&2
        exit 1
    }
    git submodule update --init iris
fi

actual="$(git -C iris rev-parse HEAD)"
if [[ "$actual" != "$expected" ]]; then
    echo "::error::Iris gitlink mismatch: pinned $expected but checked out $actual" >&2
    exit 1
fi
echo "Checked out pinned Iris $actual"
