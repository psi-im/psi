# Psi history and XEP-0313 MAM: architecture and cache policy

Status: **design / implementation plan** (2026-10-18). This document records an
incremental integration plan, not a claim that Psi already synchronizes history.
The accompanying initial patch only preserves message identity for ordinary
SQLite rows. Do not enable automatic MAM backfill or pruning at this stage.

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

## Ground truth in current Psi

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

Other MVP limitations: online EDB preload is temporarily suppressed to
avoid overlapping anonymous legacy rows; offline local preload is retained.
MAM body-only replay does not yet reconstruct all message reactions,
corrections, rich attachments, sender profile or encrypted media. Live
messages can overlap MAM results until trusted archive-ID deduplication
is wired through the existing event/storage model. This branch must remain
Draft until tested with an actual MAM-capable server and full Psi CI.

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
