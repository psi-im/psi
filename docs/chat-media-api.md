# Chat media API

`FileSharingManager::chatMedia(account, historyKey, owner)` creates a lightweight
`ChatMedia` browser for one account and conversation. Use the same
`HistoryArchive::Key` as the chat history, including its contact kind. The account
must match the key. The owner normally is the gallery widget. The factory returns
`nullptr` if the account, owner or SQLite history is unavailable, or the key does
not match the account. This API reads
locally stored history; it does not fetch older messages from MAM.

`requestPage(cursor, older, limit, callback)` returns up to 200 attachment entries.
An empty cursor starts at the newest position when `older` is true, or the oldest
when false. Pass `page.next` in the same direction to continue while `hasMore` is
true. An entry's cursor can also start traversal in either direction. The order
is message time, logical event ID and attachment ordinal, rather than archive
arrival order. Undated entries sort before dated ones. Treat cursors as opaque
values, including their `undated` field. Removing the cursor's row does not
invalidate traversal. New entries and deletions can change later pages; this is
not a frozen database snapshot.

The current message revision supplies the attachments at the original message's
position. Deleted and retracted messages and superseded attachment lists are
excluded. Each occurrence counts separately, so sending one file twice produces
two gallery positions. A page carries the publication ID, announcement metadata
(including MIME type, name, size and sources), availability and downloaded bytes.
Browsing does not instantiate `FileSharingItem`, fetch payloads, pin cache entries
or refresh their retention time.

`requestStatistics(callback)` counts complete, partial and remote occurrences with
one aggregate SQL statement on the existing history worker. There is no page
loop, message reconstruction, filesystem scan or runtime item creation for a
count. SQLite selects the account and peer through the history contact index and
resolves storage through indexed identities. Only entries selected for a page
have their announcement metadata reconstructed for presentation.

The counts and page availability use the same SQL projection. Complete means the
file registry records a complete, verified copy, including registered external
originals. Partial means a checkpoint retains some ranges on disk, including a
full byte map still awaiting verification. Remote means no registered copy or
checkpointed ranges. Volatile RAM chunks do not count as durable downloads.
Results describe committed registry state: coalesced range checkpoints can lag
an active transfer. Reading or opening content still validates the actual file;
SQL cannot detect an external edit or deletion before the storage layer does.

`stale` reports a change notification while the request was pending. Listen to
`changed()` and refresh as needed. Notifications cover history links, cache
contents and range storage; they can concern other chats in the same profile.
Debounce refreshes during an active transfer. A SQL aggregate still processes
matching rows; it is not a constant-time precomputed counter.

## SQL ownership and resource identity

History owns conversation membership: `contacts` identifies the account, peer and
contact kind, `events` identifies the message, and `message_media_refs` identifies
an attachment occurrence. The new `message_media_resources` and
`message_media_hashes` tables normalize its size, URL identity, content hashes and
account-scoped range keys. They are written in the message transaction and
cascade away with its references. The opaque message payload remains available
for display, but is no longer needed to join an attachment to its bytes.

The file registry owns physical copies, aliases, verified completion and ranges.
The history worker attaches that existing registry read-only. The aggregate
joins only the conversation's eligible references to complete cache aliases,
account-scoped range records and URL mappings. Whole-file checksum aliases also
resolve range copies when a later announcement uses a different hash. Hash aliases are alternatives;
multiple matching hashes or copies never multiply an occurrence. Retiring a file
or withdrawing its ranges changes the next result without updating counters or
running another synchronization service.

The range-key encoding is shared with the download cache in `FileIdentity` and
remains compatible with existing payloads. File schema 5 adds an expression
index for canonical hash keys used by URL mappings, explicit account scope on
range records, and a whole-file checksum lookup index. Schema 6 makes the indexed
canonical hash expression explicitly TEXT, matching URL mappings so SQLite uses
the expression index for joins. Existing version-5 profiles upgrade in place. History migration backfills
normalized identities while preserving publication IDs, attachment ordinals and
scan positions. SFS protocol reconstruction stays on the GUI before threaded
migration; SQL workers consume value-only resource projections.

Both callbacks distinguish failure from an empty result with their boolean
argument. They run on the browser's thread. Destroying the browser cancels
pending delivery; the browser also closes when its account or manager disappears.

## Opening and downloading

Resolve only the selected entry with
`FileSharingManager::chatMediaItem(entry, account)`. It uses the same `fromShare`
implementation and account item registry as `fillMessageView`, including URL-only
shares. The result is borrowed and managed by the existing file-sharing layer.
Use the existing `reader(item, owner)` for a `FileReader`, `download(item)` for a
shared full-download goal, or the chat's `FileSharingDeviceOpener` for player and
Save As integration. No gallery-specific transport or download policy is added.
Selecting an entry resolves persisted whole-file checksum aliases in the same
account to the existing range resource. Chat and gallery share that runtime item
and download controller even when their announcements use different hashes.
Only compatible, already declared hashes can select a stored copy. Conflicting
sizes or checksums cannot mutate a shared runtime item. Resolving the range
identity reads metadata without allocating a payload or extending retention;
range storage is configured when content is accessed. A complete payload can be
opened before the worker's initial snapshots arrive, using its durable checksum
proofs and validating its size and modification time. An unrelated damaged range
copy does not hide a valid complete cache copy.
Opening a reader alone does not download; reads express demand. Player backend
compatibility and abort/close ordering follow [the existing contract](file-reader-download.md).

```cpp
auto browser = manager->chatMedia(account, historyKey, gallery);
browser->requestStatistics([](ChatMedia::Statistics stats, bool ok) {
    if (ok) {
        // Display stats.complete, stats.partial and stats.remote.
    }
});
browser->requestPage({}, true, 50, [manager, account, gallery](ChatMedia::Page page, bool ok) {
    if (!ok || page.entries.isEmpty())
        return;
    auto item = manager->chatMediaItem(page.entries.first(), account);
    if (auto reader = manager->reader(item, gallery)) {
        reader->open();
        // Pass the reader to a consumer with the established lifetime contract.
    }
});
```

Successful reads and writes use the existing coarsened last-access timestamp.
Downloading more bytes therefore renews an incomplete file's lifetime; an open
reader or active full-download goal pins it against cleanup. Listing or counting
alone leaves the timestamp unchanged. The storage regression verifies that a
previously expired partial file survives cleanup after a new write.
