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

The SQLite backend's GUI-only dependencies are being separated before
moving it: `HistoryEventFactory::decode()` reconstructs `PsiEvent` values
on the GUI side, while FileSharingManager registration is performed by the
GUI request dispatcher after SQL insertion rather than inside
`EDBSqLite::appendEvent()`. Neither extraction changes the SQL transaction
schedule yet; both prepare the eventual raw-record/value boundary.

## Implemented stages in PR #1112

- The WAL checkpoint worker and read worker use `QObject::moveToThread()`
  and queued signals/slots. No mutex or shared mutable queue is involved.
  The checkpoint readiness callback has been replaced by a queued Qt signal
  so a shutdown cannot dereference a destroyed EDB facade.
- `EDBHandle::get/find` enqueue SQL reads on a read-only SQLite connection;
  the results are detached `QSqlRecord` values. The GUI reconstructs
  `PsiEvent` and delivers `EDBHandle::finished()` as before.
- Native one-to-one/MUC local scrollback now requests local pages via a
  receiver-scoped asynchronous EDB API. The MAM page continuation waits
  until the local page is projected. A dialog generation and peer check
  prevent applying a stale result after clearing or switching conversations.
- Each reader request runs in a read transaction. A local page's multiple
  original/revision lookups see one consistent SQLite snapshot even during
  concurrent live/MAM writes; the test suite covers legacy no-ID messages
  and a concurrent uncommitted writer.
- The previous EDB write queue is still on the GUI thread. Before sending a
  read to the worker it commits *preceding* queued GUI writes. Subsequent
  writes may still commit during that read; the read snapshot is isolated.
  This is not yet the final single-owner database architecture.

**Remaining work:** `EDBSqLite::appendEvent/eraseHistory/commit`, synchronous
MAM cache access/merges in `HistorySync::Context`, archive revision lookup,
displayed cursor restoration, the history contact list, and startup schema
migration all still need value-based asynchronous worker APIs. The separate
read and checkpoint connections are *intermediate*: after writes migrate,
they should be folded into one worker-owned connection and
`synchronous=FULL` reconsidered.

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

Reader operations carry a request ID and account/peer scope. Native local
page consumers additionally capture a dialog generation. The reader worker
executes its own queued requests in submission order; this is **not yet a
global ordering guarantee** with the still-GUI-owned writer. The eventual
single worker must serialize all reads and writes together.

**Target:** the UI must not wait for persistence to display a live stanza.
At this stage the UI is nonblocking for queued history reads, but synchronous
SQLite writes can still stall it. When an asynchronous EDB read arrives,
the consumer validates that the view still exists and that the request
matches its active scope/generation. An obsolete result is discarded without
rolling back any completed writes.

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

1. Preserve `EDBHandle`'s asynchronous `get/find/append/erase` contract.
   **Get/find are now worker reads; append/erase are still GUI writes.**
   Move the remaining writes behind the same queue without altering
   `finished()` semantics. Handle destruction cancels *delivery*, not a
   write already executing in the worker.
2. Convert direct synchronous consumers, notably
   `HistorySync::Context` read/merge/snapshot/lookup,
   `nativechathistory` local history/prepend (**now asynchronous**),
   archive revision resolution, `sentDisplayedCursor`, and the history contact list.
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
