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

`requestStatistics(callback)` counts complete, partial and remote occurrences.
Complete includes readable cached files, unchanged external originals and fully
verified range files. Partial means some bytes are stored on disk; a full byte
map still awaiting verification also remains partial. Remote includes files with
no retained disk bytes. Volatile RAM chunks do not count as durable downloads.
Statistics scan the same filtered history in batches of 100, with bounded memory
and a yield between batches. Its cost grows with the local conversation history; this is not a constant-time
aggregate across the separate history and file databases. It does
not create one runtime file object per occurrence. `stale` reports that a change
notification arrived during the scan. Listen to `changed()` and refresh as
needed; notifications cover history links, cache contents and range storage.
Notifications can concern other chats in the same profile; debounce refreshes
during an active transfer.

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
