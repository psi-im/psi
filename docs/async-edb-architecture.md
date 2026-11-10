# Async Psi EDB migration: architecture and invariants

## Purpose

An observed GUI hang during a long connection stopped at `fdatasync` via
Qt's SQLite driver. `QTimer::singleShot(0)` did not make the historical EDB
queue asynchronous: it still executed SQL on the GUI thread. MAM history
page writes introduced additional large transactions.

## Implemented worker path

- `EDBSqLite` is a **GUI-thread facade**. All normal `EDBHandle`
  `append/get/find/erase` requests, local chat/MUC history pagination,
  displayed-marker restoration, MAM page reads, archive snapshots,
  **atomic MAM page stores**, archived plaintext lookup, own OMEMO
  plaintext lookup, resolution of corrections, and attachment-link scans use a single
  `HistorySqlReadWorker` QObject moved to a dedicated QThread.
- The worker owns its own SQLite connection. SQL commands are processed
  sequentially as queued slots. Results are value objects (`QSqlRecord`,
  `HistoryArchive::Page`, snapshots and operation IDs), never
  `PsiEvent`/account/widget pointers. No shared mutable queues or
  `QMutex`/`QWaitCondition` are needed.
- `HistoryEventFactory` serializes events before queueing and reconstructs
  `PsiEvent` on the GUI. After successful worker commits, the publication
  synchronizer reconstructs attachment announcements on the GUI from bounded
  history snapshots.
- Live appends use bounded **worker-side batching** to preserve the old
  EDB optimization for SSD wear, CPU and WAL fsync overhead: up to three
  consecutive appends per transaction, with the original 1-second maximum wait. A
  pending batch is committed before any history read, erase, MAM page
  operation or shutdown. All per-message callbacks are emitted **only
  after the shared transaction commits**, and an error rolls back the
  entire batch and fails every pending callback. Shutdown submits the facade's
  undispatched requests before the worker stop barrier. This batching requires
  no mutexes: the same worker owns the timer and transaction.
- A worker page fetch (including multiple revision lookups) is one SQLite
  read transaction. Live appends and MAM stores share the same ordered
  worker queue; a MAM page transaction includes its events, identities,
  revisions, message-state references, adjacency and archive checkpoint.
  The GUI advances no persistent archive cursor until the store succeeds.
- `HistorySync` awaits its async cache fetch, decoded-plaintext lookups,
  own plaintext fallback, page store, and batch resolved-revision lookup
  before emitting the final `finished()` signal. SQL errors are distinct from
  successful cache misses; failed plaintext lookups stop replay before another
  decryption attempt. Ordinary live messages
  still appear without waiting for their disk append.
- A local history callback is scoped to its receiving widget; each dialog
  also validates the peer and view generation. Closing, clearing or
  repointing a view invalidates old results without reversing durable
  worker writes.
- File-share ordinals/fingerprints are serialized with the GUI-owned message
  snapshot. Version 2 indexes normalized announcements independently of body
  presentation; a separate compatibility projection reads version 1. History
  links are updated in the event transaction without parsing
  SFS/Jingle JIDs on the SQL worker. Changed links trigger catalog reconciliation
  only after commit. The synchronizer consumes bounded worker snapshots and
  advances its cursors only after successful catalog batches.
- Device-local message deletion is a worker commit barrier. Suppression aliases,
  plaintext/XML scrubbing and media-ref removal commit together; views update
  afterward. Identity-only ingress probes that miss may read the worker's pending
  append transaction without flushing it. A hit flushes that batch before learning
  aliases or hiding content. Payload projections retain their existing snapshot
  and commit-order guarantees. See [local deletion](local-message-deletion.md).
- The superseded GUI SQL `get/find/append/erase` paths, redundant query
  plans and standalone WAL checkpoint thread have been removed.
  SQLite uses **WAL + synchronous=FULL** on GUI and worker connections;
  automatic checkpoints happen on whichever connection makes the
  threshold-triggering commit, normally the worker.

## Old server identity rules

- Old servers can omit MAM entirely and also omit `origin-id` and
  `stanza-id`. Local-only history must keep working offline and after
  reconnection, including ordinary incoming/outgoing messages.
- A stable **local EDB event ID** identifies repeated reads of the
  *same stored record*. Do not interpret an absent server ID as permission
  to merge distinct messages with equal text, sender or timestamp.
- A MAM UID is scoped to its account/archive/peer; a `stanza-id` is also
  scoped by its `by` authority. Use only supported, proven identity
  edges and the existing revision graph. The SQL worker is storage, not
  a second deduplicator.
- Messages and protocol events without stored bodies must retain their
  own processing and state semantics. A failed encrypted decode is not
  durable plaintext and must remain retryable.

## Remaining synchronous compatibility/administration APIs

The application **is not yet entirely free of GUI-thread SQL**:
- Database open/schema initialization and schema upgrades, including
  UI progress for historical database migrations, are still synchronous
  at startup.
- The legacy History dialog's `contacts` and `eventsCount` methods
  execute foreground SQL. Public synchronous EDB compatibility methods
  remain, although `historysyncaccount.cpp` no longer installs those
  callbacks for normal MAM traffic.
- Internal system storage parameters and unused/legacy direct methods
  `archivePage`, `resolvedHistoryEvent`, `archivedEvent` and
  `ownPlaintext` still exist for compatibility and fallback, and must
  be reviewed as the last consumers migrate.

Do not reintroduce blocking queued connections, nested event loops,
GUI-side `QSqlQuery` in the native chat/MAM hot path, or a second
authoritative deduplication state. Complete the remaining migrations
before declaring the architectural refactor finished.

## Deterministic regression requirements

- Distinct id-less messages with identical sender/text/time remain separate;
  repeated local EDB IDs do not duplicate.
- Worker append/read/erase completion order is deterministic; an earlier
  uncommitted writer is never visible to a read transaction. A read or
  MAM store flushes preceding append batches; a failed batch rolls back
  and fails all associated callbacks.
- Local history arriving after a new live message merges into the timeline
  without deleting the live tail.
- Newer corrections and message-state transitions cannot be replaced by
  stale archive snapshots.
- Failure storing a page never advances its cursor or exposes partially
  committed rows; successful page storage returns permanent IDs.
- Cancelling a view suppresses callbacks, but never cancels an unrelated
  already committed write.
- Cached and own OMEMO plaintext reuse does not decrypt an already
  consumed ciphertext twice.
- Slow SQLite fsync/automatic WAL checkpointing takes place in the worker,
  while the GUI event loop remains responsive.
- CI across Qt5/Qt6 and Win/macOS/Linux remains green.
