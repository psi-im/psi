# Native MUC runtime regression

`muc_runtime.py` runs the **actual Psi BASIC executable** with a disposable
profile against a loopback Prosody. It enters through bookmark autojoin,
`PsiAccount::client_groupChatJoined`, `GCMainDlg`, SQLite/HistorySync and the
real native widgets. No mocked `MessageView` or manual fill-helper calls.

Build Psi BASIC with Qt 5 or Qt 6 and its normal resources installed (or point
`PSI_DATADIR` at the source tree). For example:

```sh
docker run -d --name mam-native -p 127.0.0.1:55323:5222 \
  -v "$PWD/qa/integration/mam-live/muc-prosody.cfg.lua:/etc/prosody/prosody.cfg.lua:ro" \
  prosodyim/prosody:13.0
docker exec mam-native prosodyctl register user01 localhost ci-pass-01
python3 qa/integration/mam-live/muc_runtime.py \
  --psi build/qt6_Debug/psi/psi --output /tmp/mam-native-result
docker rm -f mam-native
```

The directory passed to `--output` must not exist. The fixture seeds 80 older
bodies, 400 receipts with storage hints, and four recent bodies. Prosody sends
only those four recent bodies at join. The cold run must traverse the protocol
pages, fill 1.5 viewport heights and remain at the live tail, then stop querying.
The second run opens the same room from the verified SQLite cache and must issue
**no content-page MAM requests**. A missing/stale snapshot may require one
metadata observation (`max=1`), which does not download the old history again. `--protocol-events=0` exercises the ordinary case.

The runner records the executable's SHA-256, requires the current history marker,
and retains `cold.log` and `cached.log`. Detailed diagnostics are opt-in through
`QT_LOGGING_RULES=psi.history.debug=true`; they include identities, cursors,
counts and geometry, never message bodies. The Qt5/Qt6 CI matrix builds Psi itself
and runs this scenario; the older `client.cpp` smoke test covers Iris separately.

This is a local protocol/UI integration test. It does not establish behavior of
a public MUC with different retention, permissions or server implementation.

Build `probe/` with the same Qt major as Psi and pass `--probe BUILD_DIRECTORY`.
The test-only Qt generic plugin observes actual materialized body heights and
clipped viewport coverage, then drives the real scrollbar. `--mode fallback`
checks stock Prosody's lack of index support. `--check-metadata-upgrade` clears
only the disposable EDB's RSM metadata, reopens it, requires exactly one metadata
query and a persisted last UID/count, and checks another reopen reuses it.

The fixture uses SQLite for `muc_log`: Prosody 13's internal archive driver
omits a total even with count enabled. Metadata remains optional in Psi.

For positive indexed seek, start the fixture with `MAM_TEST_INDEX=1` and mount
`modules/mod_mam_test_index.lua` into Prosody's module directory. Run with
`--mode indexed --bodies 1200`. **This adapter is test-only**: it translates an
index into a real UID for Prosody's normal MUC MAM handler and annotates its fin.
Authorization, persistence, ordinary MAM results, paging, count and the entire
Psi path remain real. It does not establish native index support in Prosody.
No adapter is used for initial fill, cache upgrade or unsupported-index tests.
