# Async EDB migration: ownership, ordering and legacy servers

## Target

Move **all SQLite operations** for `history.db` off the GUI thread. Do not
replace a synchronous call with `Qt::BlockingQueuedConnection`, a nested
`QEventLoop`, or a timer that still runs SQL on the GUI thread.

An `EDB` facade remains on the GUI thread. A single `QObject` worker lives
on a dedicated `QThread`, owns the sole history SQLite connection, and
performs all operations as queued slots. Requests/results cross the boundary
as values with request IDs; no shared mutable queue, SQL connection,
`QMutex`, or `QWaitCondition` is required.

The current checkpoint worker in this PR is the first deployment of the
QObject/queued-slot ownership pattern. It is **not** yet the complete EDB
migration: legacy EDB and MAM still execute foreground SQL in the present
implementation.

## Compatibility with old XMPP servers

- Local EDB reads/writes and scrollback work with no MAM support and with
  missing XEP-0359 `origin-id` and `stanza-id`.
- Absence of server IDs is not permission to deduplicate by matching body,
  sender and timestamp. These can belong to distinct deliveries, including
  two separate messages with exactly the same text/time.
- A locally assigned persistent EDB event ID is still a valid identity for
  the *same local record* across repeated pages. Do not confuse it with a
  globally authoritative delivery ID.
- Server-side archive UIDs must always be scoped by account, archive and
  peer. Stanza IDs must also be scoped by their `by` authority.
- Continue using the existing EDB/HistoryArchive live/MAM reconciliation,
  including its explicit-message-ID and revision relationships. Do not
  introduce an independent deduplicator in the worker.

## Execution and presentation ordering

Each request carries `requestId`, account/peer scope and, for a view,
its current `generation`. The GUI facade may have many live requests;
the worker executes SQL in submission order.

**The UI does not wait for persistence** to display a live stanza. A queued
append cannot hold up network message display. When an EDB result arrives,
the consumer must validate that its view still exists and that its request
matches the active scope/generation. An obsolete result may be discarded
from the view without rolling back completed writes.

History pages **merge**, never replace, an existing timeline wholesale.
In particular, a live message arriving between a local read request and
its completion must survive the earlier snapshot. An older EDB/MAM result
must not overwrite a newer edited revision or reduce accumulated message
state (e.g. delivered/displayed/retracted).

## Persistence invariant

A logical MAM page is saved as **one SQLite transaction**: events,
archive-identity mapping, revision relationships, message-state updates,
adjacency and page checkpoint/cursor either all commit or all roll back.
The persisted archive cursor must never get ahead of the data it covers.
Queued incoming live appends and MAM page merges must use the **same**
SQLite worker, preserving one order of transactions without concurrent
writers across connections.

Changing the queue to prioritize reads must not allow reads to overtake
writes on which their visible state depends. If batching is introduced,
the batch boundary must retain transaction and response ordering.

## API migration boundaries

1. Preserve `EDBHandle`'s existing asynchronous `get/find/append/erase`
   contract. Replace `EDBSqLite::performRequests()`'s GUI timer with
   worker-queued operations; deliver its existing `finished()` back on
   the GUI thread. Handle destruction cancels *delivery*, not a write
   already executing in the worker.
2. Convert direct synchronous consumers, notably
   `HistorySync::Context` read/merge/snapshot/lookup,
   `nativechathistory` local history/prepend, archive revision
   resolution, `sentDisplayedCursor`, and the history contact list.
   Return typed value results asynchronously instead of blocking.
3. Move connection creation, schema checking, startup migration,
   prepared statements, timers and transaction management entirely
   into the worker. Existing migration UX must remain functional.
4. Remove the standalone checkpoint thread only after the single
   database worker owns all history reads and writes; then consider
   returning to `synchronous=FULL` without blocking GUI input.
5. Preserve the existing no-MAM fallback paths throughout migration.
   Test each consumer against a local-only server fixture.

## Required deterministic regression cases

- A local page completes after a newly delivered live message; the live
  tail stays present.
- A saved page is returned twice, including messages with no server ID;
  each persistent local event is shown once.
- Two distinct no-ID messages have identical sender/text/timestamp and
  remain distinct.
- A delayed archived original cannot replace a newer correction; markers
  never regress read state.
- Both orders of duplicate delivery (live then MAM, MAM then live) use
  existing trustworthy identity edges.
- A page-store failure does not advance an archive cursor.
- A view is destroyed/repointed while a read runs: no stale UI callback,
  no use-after-free, and no lost completed write.
- Old servers without MAM can load local history while offline, reconnect,
  and keep receiving messages.
- A deliberately slow SQLite sync blocks the worker, not GUI event
  processing. `QSignalSpy`/controllable fake sources gate completions;
  avoid sleep-based ordering tests.

Until step 3 is completed, WAL/NORMAL in this PR is a **temporary
responsiveness/durability trade-off**, not proof that all history I/O is
asynchronous.
