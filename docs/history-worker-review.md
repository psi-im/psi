# Review of the asynchronous history worker

Reviewed commit: `bf3c58bf` (2026-11-10).

The helper miss-on-error behavior and the facade dispatch queue predate this
commit. Their failure and shutdown contracts also needed attention when
reviewing the new worker boundary.

## Findings and fixes

1. **SQL failures could masquerade as successful cache misses.** The worker
   used transaction commit as the success condition, while archive helpers
   returned empty values after failed queries. A successful `COMMIT` does
   not establish that the preceding reads succeeded. This affected local
   pagination, displayed cursors, archive pages/snapshots, revision resolution
   and plaintext lookup. Local pagination could stop early; a displayed cursor
   could be considered restored despite an unsuccessful read.

   Read helpers now accept an optional `QSqlError` accumulator, including
   nested revision and adjacency queries. The worker discards the entire
   projection on error and returns failure. Successful empty reads remain
   distinct from failures, and subsequent reads can recover normally.
   Delivery matching also propagates query failures: an unsuccessful identity
   lookup must roll back the append batch rather than authorize a new insert.

2. **Plaintext lookup failures lost their status in HistorySync.** The account
   adapter discarded the EDB completion flag. That made a failed disk read
   indistinguishable from missing plaintext and could send an already consumed
   ciphertext through decryption again.

   Both asynchronous plaintext lookup callbacks now preserve success/failure.
   A failed lookup ends replay before decryption or page persistence. A failed
   lookup during cached-page preparation suppresses the remaining callbacks
   for that page, preventing duplicate completion.

3. **Exact archive hits still loaded the entire fallback window.** Unlike the
   synchronous implementation, the new worker fetched all conversation events
   within five minutes on either side of every requested archive event, even
   when its exact stored plaintext was already available. On a busy conversation
   this multiplied SQL work and cross-thread result data across a 40-event page.

   A shared, value-only stored-record predicate now lets the worker skip that
   scan for decoded exact hits. Missing or undecoded hits retain the existing
   fallback and ambiguity checks. No second identity policy was introduced.

4. **Shutdown flushed only the worker queue.** Requests still waiting for the
   facade's zero-delay dispatch were deleted after stopping the worker. The
   destructor now submits those requests before the stop barrier. The existing
   worker then commits its final append batch before closing its connection.

5. **An old displayed-cursor callback could clear a newer pending request.**
   The callback reset the pending flag before checking the view generation.
   Generation validation now precedes that mutation; same-generation peer
   changes still release their original request's pending flag.

Obsolete GUI batching fields were removed, and misleading checkpoint and
GUI-thread logging comments were corrected.

## Architecture assessment

Keep the existing single worker, sequential operation ordering, atomic page
stores, GUI-side event reconstruction, and WAL/FULL durability. These provide a
sound boundary between storage and account/presentation objects. The fixes
extend that boundary with explicit failure propagation instead of introducing
a second queue owner, deduplicator or signaling stack.

The documented synchronous startup migrations and history-browser/compatibility
APIs remain. In particular, foreground administrative writes can still wait
behind a worker transaction. Migrating their callers to the same asynchronous
interface is a separate, worthwhile next step. Bounded cached-page preparation
also still uses per-event lookups and performs a repair store; a future batch
projection API should be considered only with measurements and without losing
per-event decryption status or revision consistency.

## Regression coverage

- Injected SQL failures in twelve read paths, including nested projections;
  verified failure results, discarded partial data and recovery after repair.
- Verified that legitimate empty history remains successful.
- Verified one returned record for decoded exact archive hits and retained
  candidate fallback for undecoded hits.
- Verified rollback of a pending append batch on an identity-query failure.
- Verified persistence of a final worker append at shutdown.
- Exercised asynchronous cached-message lookup and cached-page repair together.
- Verified that lookup failures do not decrypt or persist encrypted results,
  and that late cached-page callbacks do not emit completion twice.

The facade shutdown ordering and displayed-cursor generation changes were
checked by code inspection; the worker shutdown boundary has executable
coverage. Tests do not simulate power loss or benchmark real disk latency.

Validation on Linux with Qt 5.15.18 and Qt 6.10.2:

- Both versions passed `mam_history_sql_read`, `mam_history_sqlite_policy`,
  `mam_identity`, `mam_history_source_policy`, `mam_chat_widget`,
  `mam_sparse_history_source`, `history_archive` and `history_sync`.
- Full Psi builds and the additional `chat_reply` test passed on both versions.
- `git diff --check` passed.

Qt 6.4 and Windows/macOS were not executed locally. These changes introduce
no APIs newer than the supported Qt baseline.
