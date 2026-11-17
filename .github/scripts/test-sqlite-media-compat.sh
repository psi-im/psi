#!/usr/bin/env bash
set -euo pipefail

# Test the existing Qt SQL driver with SQLite's newer expanded-cursor guard.
# Keep this test-only library isolated from the system and later CI steps.
sqlite_compat_build=${1:-build}
sqlite_compat_dir=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/psi-sqlite-media.XXXXXX")
trap 'rm -rf -- "$sqlite_compat_dir"' EXIT

curl --fail --location --silent --show-error --retry 3 \
    --connect-timeout 20 --max-time 120 \
    https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip \
    --output "$sqlite_compat_dir/sqlite.zip"

python3 - "$sqlite_compat_dir" <<'PY'
import hashlib
from pathlib import Path
import sys
from zipfile import ZipFile

directory = Path(sys.argv[1])
with ZipFile(directory / "sqlite.zip") as archive:
    source = archive.read("sqlite-amalgamation-3540100/sqlite3.c")
# SHA3-256 published at https://www.sqlite.org/releaselog/3_54_1.html.
expected = "e2b869d3003b33c942b7dd7063604fbc53553e1b66609570df39b9b7b558ee81"
if hashlib.sha3_256(source).hexdigest() != expected:
    raise SystemExit("SQLite compatibility source checksum mismatch")
(directory / "sqlite3.c").write_bytes(source)
PY

cc -shared -fPIC -O0 -DSQLITE_ENABLE_COLUMN_METADATA \
    -Wl,-soname,libsqlite3.so.0 "$sqlite_compat_dir/sqlite3.c" \
    -o "$sqlite_compat_dir/libsqlite3.so.0" -lpthread -ldl -lm

PSI_TEST_SQLITE_VERSION=3.54.1 \
LD_LIBRARY_PATH="$sqlite_compat_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    ctest --test-dir "$sqlite_compat_build" --output-on-failure --no-tests=error \
        -R '^filesharing_publications$'
