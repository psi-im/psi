# Psi history and XEP-0313 MAM: architecture and cache policy

Status: **adaptive native history with sparse navigation and durable message state** (2026-10-26).
The native timelines share the existing SQLite EDB with the history window and
legacy importer. The initial architecture analysis and longer-term retention
plans below are retained as design context; the implementation section describes
what is currently active. No automatic pruning or whole-archive background sweep
is enabled.

## Native scrollback design: MatterLeast implementation audit (2026-10-20)

The native Psi `LongListWidget.cpp` is **identical** to MatterLeast's
`sources/widgets/LongListWidget.cpp` at this review. Reuse it as the single
owner of scroll geometry, estimated row heights, semantic anchors, scrollbar
seeks, widget materialization and eviction. Do not introduce a second custom
scroll handler or geometry-driven unbounded viewport-fill loop.

### What already carries over unchanged

- `desiredRangeForViewport()` uses measured heights and a half-screen margin
  in rows. `setPrefetchScreens(0)` keeps the additional whole-screen buffer
  disabled. Demand is about a logical range, not the network page size.
- `LongListWidget::materializeAvailable()` constructs widgets only when rows
  enter the desired range. `evictOutside()` retains nearby widgets until the
  residency budget is exceeded, then destroys the farthest outside the desired
  range first. Psi's current residency budget is **160 widgets**, MatterLeast's
  `PostListWidget` sets **200**. Neither figure is a message-cache limit.
- `insertItems(..., PreserveVisibleContent)` and semantic anchors keep the
  user's viewport stable when older rows are prepended, even if individual row
  heights change after media/layout resolution. `at_end` is a separate
  sticky-tail intent, not simply `first visible == 0`.
- `LongListWidget` already understands wheel input, scroll-bar button/page
  actions, dragging, seeking and programmatic navigation. Geometry/visibility
  notifications can result from any of these or from layout changes.

### Source/view boundary

`MamHistorySource` owns resident row identities, optional event count, observed
archive windows, estimated logical positions and navigation cursors. Unknown
logical slots have no `MessageView` and no QWidget. `MamChatWidget` retains
presentation, selection and canonical message/protocol rendering. Its `items_`
reference points to the source's real rows; logical and resident indices are
explicitly translated. `HistorySync` owns one bounded EDB/MAM request and the
dialog adapters share `nativeArchivePage()` for replay projection and plugin
hooks.

The previously missing `LongListWidgetLayout.cpp` is ported unchanged from
MatterLeast, alongside the identical original cpp/header. Its existing semantic
remap transaction carries widgets, measurements and viewport anchors when sparse
positions change. Estimated-count changes remap an active seek before resizing
logical geometry, preserving its generation. No private MAM scrollbar geometry
is added.

A real gesture authorizes one navigation. Overlapping in-flight ranges share it;
a newer distant seek cancels it. Protocol-only pages continue asynchronously by
real UID until measured/estimated ordinary-message pixels meet the request:
about half a screen for scroll and 1.5 screens for seek. Repeated cursors stop
navigation. Finishing a navigation closes its request gate, so later layout or
materialization cannot start another scan. Hidden tabs pause continuations;
disconnect cancels the active range. Upward input at an unmoving scrollbar
minimum is still explicit demand. Initial fill has its own tail-preserving
pixel goal and never removes join-history or live messages.

Widget residency remains 160. Known message bodies remain resident independently
of QWidget eviction; this is not a bound on the payload cache. Very large totals
use at most one million estimated geometry slots, with proportional event
addressing beyond that. This bounds the generic dense height index without
allocating dummy messages.

Diagnostics are opt-in: `QT_LOGGING_RULES=psi.history.debug=true`. They record
account/peer, source/cache/network demand, optional coordinates, page limits,
cursor progress, inserted versus protocol rows, measured geometry and stop
reasons. They never log message bodies.

### Adaptive MAM event budget (opening and manual backscroll)

The widget counts only concrete `MessageView::Message` rows when estimating
how much actual conversation text has been loaded. It measures the real
`ChatMessageRow` heights of visible messages and falls back to measured
materialized rows (then 72 px) if no messages are currently visible.

- **Opening target:** enough *real message pixels* for the whole viewport
  plus another half viewport already cached above it (`1.5 × viewport`).
  The initial request happens before there is a useful sample; subsequent
  requests refine the mean from newly rendered rows.
- **Manual scrollback:** estimate the number of messages needed for about
  half a viewport, then ask for `clamp(15, ceil(targetPixels / avgMessageHeight) × 3, 40)`
  MAM stanzas. The factor of three allows the archive to contain both
  displayed markers and delivery receipts in addition to message bodies.
  It is a heuristic, not a statement that the server must deliver three stanzas.
- **Completion:** only an actual completed MAM/EDB page authorizes another
  opening fill check. The visible MUC backlog is never removed or invalidated.
  There is no small fixed page budget: protocol-only pages advance through
  real UIDs until the pixel target or a proven boundary. Repeated cursors and
  errors pause the demand; user input can cancel it.
- **Cache:** the chosen stanza limit is passed all the way through
  `HistorySync` and is part of the verified EDB page-read/merge contract;
  ordinary viewport demand requests at least 15 archive events; the sole
  exception is a confirmed one-event hole between stable, server-indexed
  windows. Repair that hole with `RSM index=N, max=1` only if the requested
  logical range contains exactly that one missing slot. This repair is a
  terminal operation, not a prefetch page to be continued at size one.
  `HistorySync::PageSize = 40` remains the absolute maximum.

### Approximate full archive and seek

RSM count describes archive events, including receipts, corrections and reactions.
The source estimates visible rows using sampled body/event density, refined as
windows arrive. Windows distinguish reported event positions from derived
positions. The scrollbar is an estimate, never an exact message count.

An explicit thumb seek into unknown history requests a bounded RSM index page.
Adjacent scrolling prefers actual message UIDs (not an opaque UID manufactured
from an index). An index response must report its first index; missing metadata
or an error collapses estimated geometry to known rows and reports the server
limitation in the conversation. Ordinary opaque-UID scrollback remains usable.
Count and index are optional, including on servers that otherwise implement MAM.

### Persistent snapshot and schema 0.4

`archive_snapshots` stores an account/archive/peer-scoped count, last UID, UTC
observation time and stability. This pair is a historical estimate, not proof
that the entire archive has been downloaded. A response omitting optional count
does not overwrite a useful older pair with a fabricated current value.

Old verified EDB pages can have bodies but no RSM coordinates. Replaying
these pages never triggers a separate, metadata-only MAM request. The latest
persisted snapshot remains an estimate until normal, user-driven paging supplies
fresh count and UID metadata. A missing count does not hide cached bodies;
the virtual scrollbar becomes available once a real page or retained snapshot
provides sufficient coordinates. No body-page redownload is needed for discovery.

Schema migration 0.1/0.2/0.3 -> 0.4 is additive and uses the existing blocking
migration workflow. Body/SIMS/envelope data and verified UID coverage survive.
Before-UID checkpoints remain independent of ordinal cache pages. Indexed pages
are keyed by scoped count snapshot and can serve smaller adaptive requests;
a different count invalidates ordinal cache reuse without deleting messages.
Archive erasure invalidates snapshots and both checkpoint kinds.

### Materialized message state and schema 0.5

`events.message_flags` stores the state of the canonical original message,
independently of `history_state` (archive provenance and adjacency):

| Bit | Meaning |
| --- | --- |
| `0x1` | Displayed: an incoming message was acknowledged by us, or an outgoing message by the peer. |
| `0x2` | Delivered: the recipient sent a delivery receipt. |
| `0x4` | Edited: the logical message has a known correction. |
| `0x8` | Retracted: a matching author requested retraction. |

`message_state_refs` indexes content identities and protocol targets within
one contact/account. Pending references remain available when the target has
not been downloaded yet. Effects on corrections resolve to their original row;
`resolvedHistoryEvent()` returns the latest text and that row's materialized
mask together. Consequently, an indexed page can render read/edit/retraction
state without loading the pages containing the corresponding protocol events.
MessageView carries this state through row replacement and QWidget eviction.

Live EDB appends and MAM page merges update the index and mask in the same
transaction as the payload/revision writes. Displayed markers update a prefix
of logical messages; subsequently loaded older messages inherit that prefix.
New markers extend only the unacknowledged interval. Reused ambiguous IDs do
not certify a target. An identity repair or a moved revision root rebuilds the
affected contact's projection without changing archive coverage or source data.
Direct-chat state is scoped by direction; MUC retractions additionally require
the same full sender and matching occupant IDs when available. MUC displayed
state needs per-reader data and is not collapsed into a single shared bit.

The additive 0.1–0.4 to 0.5 migration uses the existing startup consent/progress
gate and worker-owned transaction. It indexes existing envelopes and plaintext
legacy MAM references, preserving original event IDs, bodies, SIMS data, raw XML
and certified pages. Failed ciphertext is not treated as a protocol effect.
Unresolved references remain pending. No remote history fetch is required for
migration; effects never received by this client still require normal sync.

## Incremental tail and protocol replay

Reopening a connected chat uses the last UID of its persisted stable tail as
RSM `after`, with a maximum of 40 events per network page. An unchanged archive
therefore returns no events. A small delta is merged with the cached tail. If the first forward delta is
not complete, its verified links are persisted and one fresh latest page is
requested, without scanning the offline gap; explicit scroll/seek can fill it.
Older scrollback remains independently pageable. Missing
UIDs after server retention fall back to a fresh last-page request without
assuming a connection across the gap. Provisional pages remain unverified and
cannot advance a reusable checkpoint. This baseline path does not require MAM
`#extended`; newest-first ranges with an `after-id` lower bound would require it.

Receipt, reaction and retraction payloads, origin/stanza IDs, encryption metadata
and XHTML are retained in the existing extra_data JSON. Legacy readers still
understand the old OOB/SIMS fields. Old protocol-only cache records can recover
metadata from their original archive XML. DeliveryReceipt is an explicit
MessageView event: neither live delivery nor EDB/MAM replay creates a blank
message row for it. Bare protocol entries continue to count toward archive
coverage and cursors. EDB and MAM native replay share content/plugin/media
projection; rendering a correction uses the full resolved message, not a body
copied onto older attachments.

A logical row keeps its original timestamp, while revision time determines
content freshness. Reactions address the original reaction target, including a
verified room-assigned UID in MUC. Native chips aggregate complete per-actor
reaction sets, and a floating quick bar opens Psi's emoji selector. Retractions
replace content with a tombstone only for the original sender/direction.

Decrypting an archived stanza sets `EncryptionContext::setArchiveReplay()`.
OMEMO then suppresses historical key-exchange/ratchet acknowledgements. Cached
plaintext is reused before decryption; a decryption error is displayed but not
persisted as canonical body text. Stable raw XML and its archive coverage remain
cached independently of decoding: `psi:decoded=false` lets subsequent replay
retry decryption without downloading the same archive events again. This flag
uses the existing options map and does not change EncryptionContext's object layout.

Database upgrades with existing history run through HistoryUpgrade's startup
gate: explicit consent, a modal progress window, a worker-owned SQL connection,
and an atomic transaction. Declining stops startup; a failure rolls back. New
empty databases are initialized directly. Normal page SQL is still bounded
synchronous EDB work; moving that queue off the GUI thread is separate work.
This development build requires bundled Iris until the new APIs stabilize.

An eventual Iris external-cache interface should be asynchronous and scoped by
account/archive/filter/opaque cursor. It should expose raw results and stable,
complete, coverage and transaction outcomes; it must not depend on EDB, widgets
or MessageView. Storage policy, local-only records and user projections remain
application responsibilities. The current adapter remains in Psi; that broader
Iris cache interface is not implemented by this change.

## Implemented EDB/MAM bridge

`HistorySync` is the account/application adapter over Iris MAMTask. Each request
has a 40-result limit. It checks the persistent EDB cache first, authenticates
conversation/room ownership again when reading cached XML, and merges successful
server pages directly into EDB. Replays never enter the live notification/event
queue or any file-based history mirror. Both direct and room native timelines use
this adapter; the WebKit chat path continues to use the existing EDB preload.

SQLite schema **0.2** adds `events.history_state` and `history_date_ms`, plus
`archive_items`, `archive_pages` and `archive_page_items`. Migration is one
transaction, keeps all original event IDs, payloads, dates and `extra_data`, and
does not run legacy flat-file history import. UTC millisecond ordering
keys are filled in batches, preserving original date strings. This avoids
lexically misordering MAM UTC stamps and old desktop-local timestamps. Rows
written by an older client after downgrade receive their ordering keys at the
next migration pass. Unknown future schema versions are refused without edits.

`HistoryArchive::State` is an enum bit field:

- `Unverified = 0`: the default for legacy, imported and new live messages, and
  provisional MAM results.
- `Archived = 1`: confirmed by a successful stable MAM result.
- `LocalOnly = 2`: proven local events; authentication/subscription records can
  be classified directly because they are not MAM message stanzas.
- `LinkedPrevious = 4` / `LinkedNext = 8`: proven archive adjacency. Provenance
  uses `ProvenanceMask = 3`; adjacency is independent of it. Exact scoped UID
  neighbours are also retained in `archive_items`, with NULL meaning unknown
  and an empty previous UID denoting the proven start of the archive.

A missing local message in one server page, an error, a date coincidence or a
retention-limited archive does not prove that an old message was never archived.
Ambiguous records remain unverified and are retained. Correlation requires a
unique matching client ID, original sender, direction, type, body, subject and a
bounded timestamp difference; IDs or bodies alone never collapse originals.
Matching preserves the local event ID and payload. The reverse race, where live
logging follows an already stored MAM result, uses the same delivery check.

Cache identity is scoped by local account ID, archive bare JID and peer filter.
Stable page events, identities, neighbour links and checkpoints commit together.
No reusable checkpoint is written for `stable=false`, omitted privacy-hinted
results or failed parsing. Deleting event rows invalidates their cached pages;
EDB history erasure also removes empty boundary checkpoints. A moving tail is
always refreshed online on opening/reconnecting. Older pages skip MAM only if
all requested rows are covered, or a stable response proves the archive start.
Rows can be assembled along verified neighbour links across old page boundaries,
so incoming tail messages do not force all previously loaded pages to be fetched
again. Opaque UIDs are never sorted or interpreted as local row numbers.

Local EDB keyset pagination has its own `(history_date_ms,id)` cursor. It remains
available offline and after MAM exhaustion/failure, preserving local originals
that no longer exist on the server. Native rows adopt durable EDB IDs when MAM
and live deliveries overlap. Archive page order takes precedence for equal
stamps; merging retains local-only rows and whole-message selection identities.

Public room cache contacts use `EDB::GroupChat = 3`, separate from the existing
`GroupChatContact = 2` occupant-private history. The history dialog lists cached
rooms under **Group chats** and displays occupant nicknames. Persistent MAM
writes respect the account's logging option and message storage hints. Ordinary
public room live logging policy remains unchanged; fetching a room's MAM page
persists that page in the shared EDB.

SQLite regression tests cover old-schema migration and rollback, payload
preservation, timezone ordering, equal-stamp keyset pages, provenance and
adjacency, shifted boundaries, gaps, unstable results, empty complete pages,
ambiguous deliveries, account/archive isolation, explicit deletion and storage
hints. The native widget tests cover EDB ID adoption, mixed local/server ordering,
overlapping equal-stamp pages and retained selection. Both suites run on Qt5 and
Qt6; the integration build also compiles the actual EDB and Iris adapter.

## Sources and standard requirements

- [XEP-0313 v1.1.3](https://xmpp.org/extensions/xep-0313.html), §§2–6:
  the standard explicitly supports synchronization, on-demand calendar queries
  and infinite scrollback. It **does not** mandate permanent full local mirrors
  or a particular local cache size.
- [XEP-0359](https://xmpp.org/extensions/xep-0359.html): trust a stanza ID as an
  archive ID only for an archive JID that advertises MAM and whose ID issuer
  can be authenticated. MAM `result/@id` is the archive UID for query results;
  it must not be confused with a message's ordinary `id` or `origin-id`.
- [XEP-0059](https://xmpp.org/extensions/xep-0059.html): bounded, opaque
  server-side RSM cursors; `<before/>` requests the newest page.
- [XEP-0441](https://xmpp.org/extensions/xep-0441.html): archiving preferences
  are experimental and **not encouraged** when they disrupt sync.
- [XEP-0045](https://xmpp.org/extensions/xep-0045.html): room join history is
  independent of MAM; clients can receive both copies.
- [XEP-0334](https://xmpp.org/extensions/xep-0334.html): message processing hints
  and user logging/privacy choices must be honored.

XEP-0313 §3.2 lets servers delete *oldest* entries due to retention limits.
Once a server drops a message, only a saved local copy may survive. A client
must not assume that old data can always be fetched again. The archive may
also contain moderation tombstones rather than the original content.
The `fin/@stable=false` response means the returned data should not be
cached as permanently verified without re-query. `fin/@complete=true`
completes the requested result window; it **does not** mean the client has
downloaded or permanently secured the whole archive.

## Initial architecture inventory (before the bridge)

- `src/edbsqlite.cpp`: one SQLite `history.db` with `contacts` and `events`.
  Event fields are date, direction, type, body, subject, language and
  `extra_data`. There is no MAM UID, trusted archive JID, coverage interval,
  or uniqueness key. `events.id` is a local integer unrelated to MAM.
- `EDB/EDBHandle` is asynchronous at the API boundary but is date-oriented,
  and SQL queries order by `date`, not by archive order. Equal timestamps
  occur legitimately in MAM. Range completeness cannot be inferred from
  first/last timestamps or local row counts.
- `HistoryMessageExtras::encode()` formerly preserved `id/from/to`
  **only when media references were present**, so ordinary history could
  lose the ID and original sender resource. This first PR fixes that for
  newly persisted rows, keeping old JSON (and rows without JSON) readable.
  An ordinary stanza `id` is not a verified MAM UID; do not deduplicate
  solely on it.
- `ChatDlg::preloadHistory()` fetches a bounded local tail using EDB;
  `HistoryDlg` exposes date-based/offset-based paging and text search.
  Neither can distinguish an archived gap from a genuine end of history.
- `PsiAccount::handleEvent()` intentionally does not log public groupchat
  messages to EDB; sending groupchat also explicitly skips local logging.
  `GroupChatContact` is an existing type for **MUC private chats**, not
  a general public-room archive. Room caching needs its own classification.
- MUC joins currently request XEP-0045 join history using
  `groupChatJoin()` options. Mixing that history, live messages, carbons,
  and MAM without idempotent insertion would produce duplicates.
- `dependencies.lock.json` currently pins Iris **v1.3** and Psi uses the
  Iris git submodule. The newly merged MAM/ID APIs (#118–#123) must first
  be made available in Psi's dependency path; don't introduce references
  to non-pinned APIs or mix an old system SDK with a new submodule.

## Proposed policy

**Direct / 1:1 conversations:** retain local SQLite messages indefinitely,
except for explicit user deletion or configured no-history behavior. Catch up
with MAM on reconnect and lazily fill missing older pages. Never interpret
`complete` as permission to stop *future* catch-up after new remote messages.

**Public MUC rooms:** persist a bounded, disposable cache of *verified
MAM-backed rows* per room, initially **5,000 messages** (a configurable,
non-standard policy), with an additional global disk budget for large numbers
of rooms. Do not purge imported records, unsent/outgoing-only records,
locally unique events, explicitly protected messages, or any row without a
verified MAM replacement. Do not apply this cap to private MUC messages
(they belong to the private-conversation policy). On eviction, invalidate
coverage accordingly; prefer least recently accessed unprotected intervals
and never evict a page currently displayed or pending use. Cached room history
must work offline, including previously fetched noncontiguous windows.

**An explicit durability trade-off:** "verified MAM-backed" means that the
server had an item at fetch time, **not** that it promises future retention.
After local MUC eviction, a server may delete that item before the user
revisits it, so older room history may be irretrievably lost. Expose this
choice in settings (bounded cache versus keep downloaded room history);
do not promise recoverability. A global budget is a soft bound over
*evictable* rows only, excluding locally owned/protected messages.

**No MAM, no authorization, offline, or failed query:** return available local
rows and mark the requested range as unavailable/unknown; no blank-screen
regressions and no infinite retry loops. Never silently claim a complete
history when the archive cannot be checked.

**Privacy:** respect existing `opt_log`, manual clear/delete and no-store
handling. Explicit deletion must not cause automatic resurrection on the next
sync. A deliberate user-triggered `load remote history` may use a separately
confirmed policy where local persistence is disabled, but must not bypass
privacy settings. Existing local data predating MAM are user-owned data,
not implicitly disposable cache.

## Architecture: one message store, sparse coverage

Keep `EDBSqLite::events` as the single canonical message storage.
Do not build a parallel full message database in an Iris plugin.
Iris remains transport/validation (MAMTask, MAMManager, Message,
Forwarding, RSM); Psi owns persistence, synchronization and rendering.

Add a dedicated `HistorySyncController` scoped to each account (or an
equivalent narrowly focused component), with `ArchiveKey(account_id,
archive_bare_jid)`. `ArchiveKey` identifies the *server archive*: the
account's bare JID for direct chat, the room's bare JID for MUC. A direct
conversation is a filter/view over the **same account archive**, not a
separate server archive. Preserve account ownership even for identical room
JIDs connected through different Psi accounts.

Migrate SQLite in small transactions; proposals to evaluate in the next PR:

- `mam_message_identity(account_id, archive_jid, archive_uid,
  event_id REFERENCES events(id) ON DELETE CASCADE, provenance)`, unique
  by `(account_id, archive_jid, archive_uid)`; add a separate unique
  event mapping if needed. `archive_uid` is opaque; never sort it
  lexically or numerically.
- `mam_coverage(account_id, archive_jid, filter_jid, first_uid, last_uid,
  before_complete, after_complete, stable, ...)`, storing verified
  *adjacent* page windows and their continuity, not a simple min/max
  timestamp. A scoped `with` query's coverage is not interchangeable
  with an unfiltered archive's coverage. Eviction splits/removes affected
  intervals. A `stable=false` page is provisional until validated.
- A persistent per-archive **catch-up checkpoint** distinct from the
  current newest row. Advance the checkpoint *atomically with committing*
  its contiguous range; a crash or page timeout must never skip
  intermediate messages. The high-water mark of a failed batch must
  remain unchanged.
- Record cache provenance (live/MAM verified/local-only/legacy imported),
  local-delete suppression and protection. This prevents pruning local
  originals and inadvertent resurrection.

Do not reuse the historical `events` date index as a MAM order. Within a
validated MAM page, persist received order; RSM first/last identify page
boundaries. Equal timestamps must have a deterministic local tie-breaker.
Opaque archive UIDs are never arithmetic cursors. Timestamps may only
be *hints* for querying or matching legacy rows. A missing local interval
must not automatically be deemed missing from the server archive.

### Sync requests

1. On account connect: discover MAM at the **account bare JID**, and
   per-room MAM at each **room bare JID**. Cache disco capabilities per
   session; don't assume `#extended`, `include-groupchat` or room access.
2. Start from a committed checkpoint or use a bounded time overlap where
   no trusted UID exists. Fetch chronologically forward for catch-up;
   deduplicate against live/carbons/SM and existing archived rows before
   inserting. Query id and archive sender must be validated by Iris.
   A per-conversation tail opened in the UI may use `with` on the
   account archive but must not advance the global catch-up checkpoint.
3. On first chat open: render immediately from EDB, then ask for a
   bounded last MAM page where no verified tail coverage exists.
4. On reverse scroll/history navigation: serve a cached *contiguous*
   interval first. Fetch `before <first-known-UID>` via Iris
   `MAMManager::getMessagesBeforeID` for uncovered ranges; merge verified
   pages into the existing event table. Use `#extended` range/single-item
   requests only when disco explicitly allows them. For legacy data
   lacking UIDs use bounded time-overlap fallback with conservative
   matching, never destructive dedup by body/timestamp.
5. `MAMTask::setCollectResults(false)` and `resultReceived` support
   streaming without retaining an entire archive in memory. Batch disk
   writes; commit page coverage/checkpoint only once a successful stable
   `fin` confirms the relevant page. A canceled task must release
   subscriptions and roll back unverified progress.
6. Coalesce same-archive/range requests (single-flight), cap total
   in-flight pages (e.g. two account-wide) and prioritize visible
   windows. Cancel when account disconnects or the last consumer leaves.
   Don't start a background full-history sweep automatically.

### User interface and coexistence

- Prefer a reusable history source facade under `ChatDlg`,
  `GCMainDlg` and `HistoryDlg` rather than independent MAM clients
  within each widget. Qt text and Web chat views require separate
  prepend/viewport-position handling; add transport-independent
  tests before changing either renderer.
- Preserve the visible scroll anchor, cursor and selection when prepending.
  Never replay MAM messages through `PsiAccount::handleEvent()` as *live*
  events: no duplicate notifications, unread counts, sounds, tabs or
  auto-follow/correction side effects.
- Persist ordinary bodyless semantics as needed for full replay
  (reactions, corrections, moderation tombstones, IDs, rich media
  references, and original sender/room occupant identity). Current
  `events` message payload is lossy and requires deliberate extension.
- MUC XEP-0045 join history and live MAM overlap are deduplicated
  through trusted archive IDs where available. Keep room public history
  separate from private occupant messages. Synchronization should
  not make a room appear joined or show messages inaccessible to the user.
- Local search reflects locally available/retained ranges; do not claim
  it covers server history without a separate MAM search facility.

## Implementation sequence, each as a separate Draft PR

0. **This PR:** persist ordinary message `id/from/to` in existing
   `extra_data` without schema changes and regression-test legacy rows.
   No network behavior or cache eviction yet.
1. Upgrade/pin Psi's Iris dependency to a released MAM-capable version,
   or temporarily consume an approved exact Iris commit for development;
   establish dedicated offline fake-MAM fixtures. No silent release-tag
   movement and no unreviewed dependency bumps.
2. Add SQLite migration, verified message identities and account-/room-
   scoped coverage/checkpoints, plus transaction/crash/reopen tests.
   Include old history.db migration fixtures, duplicate and equal-time
   scenarios, and suppression of auto-refetch after explicit deletion.
3. Add account catch-up with sender validation, capability gates,
   resumable cursors, legacy overlap and idempotent inserts. Test
   overlapping live/offline/carbons, failed pages and reconnect.
4. Implement cached MUC room history and bounded quota with protected
   original rows, offline behavior and safe eviction/coverage invalidation.
5. Implement local-first older-page loads in history UI and chat views,
   preserving scroll anchors and user-visible partial-history states.
6. Stress-test long archives, many rooms/accounts, limited MAM retention,
   unstable pages, UID expiry, query cancellation, reconnect, disabled
   logging, OMEMO, moderation tombstones, and page concurrency. Profile
   SQLite indexes, cache growth and write amplification.

### Non-goals of this initial PR

No destructive migration, arbitrary 5,000-row DELETE query, automatic
whole-archive download, per-message MAM network requests, or force-merging
the server archive onto legacy history. Those are easy ways to lose users'
existing messages or produce duplicates.

## Native BASIC chat MVP (experimental, PR #997)

The initial widget implementation lives in the dependent Draft PR #997
(`ai/mam-native-chat-scrollback`). It uses the real MIT-licensed
`LongListWidget` virtualization/viewport-anchor engine and LGPLv3
`OverlayScrollBarManager` from MatterLeast, plus a small Psi-specific
`MamChatWidget` that renders actual Qt row widgets. The overlay is enabled
only on the new view. Widget-only CI runs original MatterLeast behavioral
tests on Qt5 and Qt6, including scroll anchors, resizing and edge actions.

The BASIC `PsiChatDlg` currently replaces only its visible presentation;
`chatview_te` is still a hidden compatibility adapter for the rest of
Psi's chat API. It must not be removed until selection, correction/retraction,
reactions, file and message context menus, trackbar and custom rendering are
ported. WEBKIT/WEBENGINE view types are explicitly excluded from the BASIC
replacement.

A `MAMManager` query fetches at most 40 archived results. The first page
is shown newest-last; scrolling to the top requests the preceding page
using the server's **opaque archive UID**, not a timestamp or arithmetic
index. A current request suppresses duplicate requests; completed results
prepend without moving the visible semantic anchor. MAM replay is dispatched
only to the view, never through the live PsiAccount event pipeline.

### Important runtime and release restrictions

Psi currently locks Iris v1.3, whose old MAMTask ignores normal
`<fin/>` termination and requires non-portable extended metadata.
The MVP is therefore **not functionally qualified on the release pin**.
The special `mam-native-integration.yml` job builds BASIC against
the known post-audit Iris revision
`292b150f8e97fa480a0def4ae6cd25abc21a1529` for development
only, without silently changing the release lock. A future audited Iris
release+gitlink/lock update is necessary before enabling this in production.

Remaining native limitations include full reaction/retraction handling,
sender profile reconstruction and encrypted replay. EDB preload, media
references, persistent UID reconciliation and page caching are now connected
through the bridge described at the start of this document. Ambiguous old
messages remain unverified. This branch must remain Draft until tested with an
actual MAM-capable server and full Psi CI.

### Transitional BASIC chat compatibility and live/MAM overlap (WIP)

The native widget timeline now correlates a live delivery with a subsequently
returned MAM result only within its current conversation and only when original
stanza ID, direction, rendered text and timestamps (within five minutes) agree.
A match adopts the authenticated archive UID without inserting a duplicate row.
The ordinary stanza ID is **never** treated as a MAM cursor or server UID.
SQLite rows still need origin-ID/stanza-ID provenance and archive coverage for
complete durable reconciliation; the heuristic must not delete any local record.
The hidden QTextEdit adapter continues to receive live message updates while
native reaction/retraction and composer interactions remain unfinished.

An interrupted MAM task is cancelled when an account disconnects. If the
archive fetch was in flight, reauthentication restarts from the same opaque
server cursor regardless of how many rows the native timeline already contains;
this does not clear or scroll the current viewport. Initial MAM discovery is
still skipped when offline-only SQLite rows have already populated the view,
until durable identity/coverage reconciliation is implemented.

If a chat is opened offline and its SQLite tail is still loading when account
authentication finishes, the first MAM request waits for the local preload
completion. A nonempty local result remains displayed unchanged until proper
archive-UID reconciliation is available; an empty result permits normal MAM
startup. The native chat controller never races two unrelated history sources.

### Guarding the offline SQLite-to-MAM transition

The BASIC timeline now remembers whether asynchronous local EDB history has
**actually populated** the visible chat. Both an in-flight local query and a
completed nonempty local result suppress **every initial** MAM fetch, including
one requested manually by scrolling to the top after reconnect. Later server
pages with an established archive UID are unaffected. The flag resets when the
native timeline is cleared or the bare chat peer changes.

Every local EDB query stores the originating full chat JID; its completion
drops historical rows when the dialog has changed peer before the callback.
The preload completion signal still releases held live messages. This avoids
showing the previous contact's SQLite records in the next conversation.

This is an explicit conservative boundary, not a completed SQLite/MAM merge.
Durable identity and archive-coverage reconciliation must precede mixing
the two sources or using the server to evict any local message.

### Live MAM network acceptance (Prosody, WIP)

The independent `mam-prosody-runtime.yml` workflow starts an ephemeral local
Prosody with `mod_mam`, logs in with the same development Iris MAMManager
API used by BASIC, sends 85 messages and verifies three chronological
backward pages of 40, 40 and 5, including unique opaque archive UIDs, peer
identity and last-page `isComplete()`. Unlike compile-only CI this exercises
a real XMPP connection, SASL, server MAM storage, streamed results and RSM
page boundaries. It does **not** instantiate the complete Psi BASIC chat
window; a separate end-to-end native UI test is still required. Runtime
qualification must wait for this workflow to pass.

### Native QWidget regression coverage

In addition to upstream `LongListWidget` virtual scrolling and overlay
scrollbar tests, the Qt5/Qt6 `mam_chat_widget` target compiles the real
`MamChatWidget.cpp` and checks physical `QFrame`/`QLabel` creation,
message correction in place, adopting archive UIDs for already shown live
messages, bounded materialization after inserting 40 older rows, and
continuation when a nonterminal page contains no visible messages. It uses
a deliberately minimal test-only `MessageView` contract to avoid injecting
a fully initialized Psi account into a presentation test; full BASIC CI
separately compiles against the actual `MessageView` implementation.

### Multi-message copy (MatterLeast selection model)

A mouse drag that leaves its initial row enters whole-message range selection;
ordinary drag within one text label keeps Qt's normal text selection. Ctrl+click
toggles an individual message, Shift+click selects a contiguous range, and a
floating Copy / Cancel toolbar appears with the selection count. Ctrl+C copies
the selected *messages*; Escape exits the mode. Clipboard blocks include sender,
local timestamp and plain message body, in chronological order. Selected
identities live in the model rather than the materialized widget collection, so
virtualization, prepending old archive pages and live-to-MAM UID promotion do
not lose the selection. The original MatterLeast bulk deletion action is not
ported because Psi does not yet expose its equivalent safe operation.

### Native themes and conference presentation

BASIC personal chats and conference windows now use the same `MamChatWidget`.
The old ChatView stays hidden for the remaining compatibility APIs; WebKit and
WebEngine keep their existing theme providers. In Appearance → Themes, BASIC
has separate Chat Message Style and Groupchat Message Style selections:

- **Modern** (default): avatar column, bold author, time beside the author, body below.
- **Corrections (XEP-0308)**: the visible message keeps one logical row with
  all its stanza revision IDs even when EDB/MAM pages arrive after a live edit.
  MAM's archive UID is a distinct source key and does not identify the
  corrected content. Revision ancestry and sender direction are used for
  reconciliation; raw peer JIDs versus contact display names are not distinct
  senders in a 1:1 conversation. Each edited row shows a pencil after the
  timestamp, with a correction tooltip.
- **Classic**: compact inline `[HH:mm:ss] Nickname: message` rows with wrapped rich text.
  Modern remains an avatar/header-over-body layout with relative timestamps.
  Groupchat nickname colors in both styles use the existing Psi MUC options,
  hash/palette policy and uncolored-history preference.

Both layouts use the application palette for ordinary text, the configured chat
font, and the existing AvatarFactory cache (occupant avatars for conferences).
Missing avatars use an initial. Ordinary message timestamps update every second: seconds,
minutes or hours ago for ages below 24 hours, otherwise `HH:mm` in local time.
Future timestamps also use `HH:mm`. Hovering the time always shows a full local
date and time with seconds and the time zone. Timestamp refresh touches only
materialized labels, without rebuilding the list or its logical identities.

Plugins can implement `NativeChatThemeProvider` from
`plugins/include/nativechatthemeprovider.h` and declare it in `Q_INTERFACES`.
After a successful enable, Psi copies each theme description (unique ID, name,
Modern/Classic base layout, avatar size, spacing, row stylesheet, optional
nested JSON `layoutSpec`) into the native
theme registry. A layout tree has `direction` (`horizontal`/`vertical`)
and `items` (nested groups or any ordering of `author`, `timestamp`,
`avatar`, `body`). Each leaf is unique and `body` is required; invalid
definitions fall back to the native Modern/Classic layout. IDs `modern`,
`classic` and `bubble` are reserved; duplicate IDs are ignored. Stylesheets may
target `mamMessageRow`, `mamAvatar`, `mamAuthor`, `mamTimestamp` and `mamBody`.
Use palette colours to support light/dark desktops. String data is explicitly
detached from plugin static storage. Disable/unload removes the plugin's
themes, refreshes open settings and views, and falls back to Modern if the
selected theme disappeared. The saved ID is retained for the next enable.

For full control over the appearance of each message, a plugin can also
implement `NativeChatRendererProvider` and arrange Psi-built message parts with
its own widgets; see [native-chat-renderers.md](native-chat-renderers.md). The
built-in Modern, Classic and Bubble themes are renderers on the same API.

Conference MAM requests start after joining and address the room's bare JID,
without a `with` filter, as required by [XEP-0313](https://xmpp.org/extensions/xep-0313.html).
Pages use Iris MAMTask, 40-result limits and opaque backward cursors. Replays
never enter the live event/notification/storage path. Missing/forbidden room
archives leave the ordinary room history visible; disconnect cancels the pending
query and rejoining resumes its cursor. Clearing a conference cancels loading
and suppresses immediate history replay. Correlation of live and archived rows,
and message corrections, require the same occupant nickname in addition to the
existing delivery checks: different occupants can reuse the same stanza ID.
Durable EDB reconciliation and media references are now connected as described
above; the remaining native reaction/retraction/composer APIs are still pending.

Message bodies use `ChatMessageText`, a scroll-free QTextBrowser with the existing
PsiRichText parser and inline object handler, as used by PsiTextView. A plain
QLabel cannot display Psi's `<icon>` emoticons. Copying partial text or whole
messages converts inline icons back to their textual emoticons. The body and row
report document height for the available width, rather than QTextEdit's editor
size or QLayout's wrapped height at its narrower preferred width. Header and body
are aligned at the top without vertical stretch. Width probes restore the
painted document width, preserving selection and wrapping after window resizes.
Widget regressions exercise both layouts through narrow/wide/narrow geometry,
actual inline-icon painting, and both clipboard paths using the real PsiRichText
implementation with only the icon asset provider substituted.

While whole-message selection is active, both native layouts show a checkbox
column on the left, including unchecked controls for the other messages. Clicking
a checkbox updates the model-backed selection; cancel hides the column. Newly
materialized rows restore visibility and check state from the logical selection.
Checkbox callbacks resolve the row's current key instead of capturing an index,
so prepending history or adopting a MAM UID cannot toggle the wrong message.
Selection is cancelled before clearing model entries, while existing rows can
still safely synchronize their controls.

Whole-message selection colours both the row Window and the document Base
surface, including on dark palettes. Entering this mode clears text selections
in every materialized row and temporarily disables partial text selection and
link activation; cancelling restores normal text interaction. This also applies
to newly materialized rows. Regressions check actual painted document pixels
and a drag that starts as partial text selection and crosses message boundaries.

Only ordinary messages have author/time headers. Informational/system rows are
centred; subjects are left aligned. These rows have no visible clock and expose
the full timestamp on the row/body tooltip instead. Their foreground comes from
`options.ui.look.colors.messages.informational` through the existing ColorOpt
contrast adjustment. Changes to this option or the application palette update
open views without rebuilding the conversation model.

### Desktop palettes, fonts and native chat input

Appearance → Fonts & Colors edits independent **Light desktop** and **Dark
desktop** colour sets, including chat background and ordinary text. Switching the
editor selector preserves pending edits to both sets; Apply stores both. Runtime
selection is automatic, based on Qt's desktop Base colour, and never overwrites
the application-wide Qt palette. Invalid colours inherit the corresponding Qt
palette role. Explicit defaults are chosen separately for light/dark desktops.

Storage is `options.ui.look.palettes.light.*` / `palettes.dark.*`. The old single
`options.ui.look.colors.*` storage is intentionally not migrated. Those names
remain logical identifiers for ColorOpt consumers and WebKit theme scripts;
ColorOpt and the JavaScript settings bridge resolve them against the active
set. Qt desktop palette changes reload every colour, notify existing consumers,
and update native chat viewports and materialized rows, including explicit
backgrounds. The existing WebKit settings notification path also receives the
resolved colour changes. Groupchat nickname-colour lists and tooltip/tab colours
use the same active storage.

Changing the chat font updates existing document fragments and nickname labels
in place and remeasures their rows, rather than relying on document default font
inheritance or replacing message rows. Explicit fragment sizes left by the rich
text parser would otherwise retain the old size. Font changes preserve emphasis,
inline object metadata and logical selection. A normal groupchat nickname click
uses the conference's existing nickname insertion handler; text dragging and
whole-message selection do not insert nicknames.

Overlay hover keeps a periodic fade check running. This prevents a lost Leave
notification after a geometry/window transition from permanently pinning the
scrollbar. Fading still respects an actual thumb drag or pointer over the
controls; keyboard focus changes alone do not change that hover policy.

Reveal/fade transitions also cancel an opposite animation before taking the
already-at-target shortcut. Otherwise leaving before the first reveal frame
would leave the reveal running after a zero-opacity fade, making the bar visible
again. The scrollbar suite reproduces this race and a lost Leave notification.

Palette refresh also recolours link fragments already stored in message
documents. Native rows and the legacy text view share PsiRichText's link
recolouring helper; inline foregrounds produced by linkify on a light desktop
must not survive unchanged after switching to a dark palette.

### SIMS compatibility and native inline media

The SIMS history change (`eec9b456`, followed by `c3ec21c1`) extended JSON in
`events.extra_data`; it did not change the SQLite schema. Existing OOB-only JSON,
SIMS reference XML and the v1 message envelope remain readable. Schema 0.2 keeps
that field byte-for-byte, so SIMS does not need another ALTER or data rewrite.
The independent account-scoped `shared-files.sqlite` catalog creates its own
`shared_files` table and is not a replacement for EDB history. A disk-backed
regression now migrates both SIMS and old OOB rows through the actual 0.1→0.2
migration and checks sender/recipient, reference sources and sizes above 2^53.
Metadata that was never stored by older versions cannot be recreated by a schema
migration alone.

`ChatMessageText` now derives from `PsiTextView`. Full document replacement goes
through the same icon, `<share>` and marker parsers as the classic view. Native
rows receive their account's `FileSharingDeviceOpener` before parsing their body,
including recreated rows after theme changes and archive/local history replay.
Audio/video use the existing qite controllers, images use the existing shared
image path. Document measurement, font/palette updates and outer timeline scrolling
remain in `ChatMessageText`/LongListWidget. Audio/video state and streams belong
to the row's existing media controllers; deleting a virtualized row destroys
them. File cache/download state stays account-owned. Video display remains lazy
and does not open the source merely to construct a row.

The widget regressions now compile the real PsiTextView, PsiRichText and qite
implementations. Only account storage/network resolution, icon assets and external
URL launching are substituted. They check audio/video/image object registration,
account-scoped source URLs, document replacement and media row recreation, in
addition to the text/font/layout/selection tests.

qite is an interaction/media layer over Qt text objects, not a prerequisite for
inline drawing: [QTextObjectInterface](https://doc.qt.io/qt-6/qtextobjectinterface.html)
provides custom size/draw hooks directly. Animated PsiIcon emoticons can use their
existing frame-change notifications with balanced activation/stop and targeted
viewport updates. They do not require a new text layout engine or media player;
this change restores SIMS and does not implement that animation lifecycle.

### Persistent corrected-message resolution across EDB/MAM page boundaries

SQLite records keep original payloads. The message_revisions index records
contact, sender, direction, stanza and replace IDs, and resolves the earliest
revision as the ordering anchor. Live EDB appends and MAM page transactions
update this graph; migration indexes old envelopes and cached MAM XML.
The local history page selects logical rows by original date but resolves
the latest known revision from the same database, even if it belongs to a
different archive page. Archive UIDs, XML and cursors remain per revision.
MAM pages use resolvedHistoryEvent(eventId) for consistent contents.

Only edits already known locally can be resolved offline. MAM tail refresh
on reconnect remains essential for corrections not yet received. Outgoing
new messages scroll to the tail; corrections of old rows do not.
