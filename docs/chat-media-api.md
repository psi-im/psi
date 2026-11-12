# Chat media API

`FileSharingManager::chatMedia(account, historyKey, owner)` creates a `ChatMedia`
browser for locally stored conversation media. The key must match the account;
include the contact kind and the full occupant JID for a private room chat. The
factory returns null if the account, owner, key or SQLite history is unavailable.
Browsing does not fetch older messages from MAM.

## Resources, filters and navigation

Each position represents one resource referenced by the conversation. Repeated
announcements and compatible hash aliases do not create additional positions.
Account-scoped URL-to-content mappings also join address-only announcements to
known content. That equivalence survives payload eviction; availability changes
without multiplying positions. Unrelated files with identical names remain
separate. Messages contribute membership only: deleted, retracted and superseded
attachment lists are excluded. Removing the last eligible reference removes the
resource from the conversation.

`setFilter(MediaFilter)` selects independent bit masks:

- `types`: `Images`, `Videos`, `Files` (other files).
- `availability`: `Remote`, `Partial`, `Complete`.

Both masks default to all values; an empty mask selects nothing. The same SQL
selection determines pages, statistics, and each entry's one-based `index` and
`total`. A verified copy's MIME type takes precedence over announcement metadata;
a filename extension supplies a fallback for address-only links. Partial means
checkpointed ranges on disk, including a complete byte map awaiting verification.
Complete means a registered verified copy. Remote means neither is registered.
Volatile RAM bytes do not count as durable downloads.

`requestPage(cursor, older, limit, callback)` returns up to 200 entries. An empty
cursor starts at the last resource when `older` is true, or the first otherwise.
Pass `page.next` in the same direction while `hasMore` is true. Resources have a
stable registration order independent of message timestamps. Cursors are opaque
resource positions; deleting the cursor's occurrence does not break pagination.
Merging previously unrelated aliases may change their canonical position; this
is a live view rather than a frozen snapshot.

`requestEntry(sourceId, callback)` locates the resource opened from a chat without
recovering a message or guessing a timestamp boundary. The identifier is the
same content hash or URL identity used by the chat's `share:` link.
`requestAdjacent(sourceId, older, callback)` resolves that anchor and its neighbor
in one SQL statement, including when the anchor is excluded by the current
filter. The overload accepting an `Entry` also retains its opaque cursor as a
fallback if the last reference is deleted while the file is open. Entries expose `name`, `mime`, `type`, `storage`, `index`, `total` and
`sourceId`, plus an opaque cursor and the announcement needed by the file layer.
Consumers do not need to interpret protocol metadata to navigate.

`requestStatistics(callback)` returns `complete`, `partial`, `remote` and `total()`
using one aggregate statement. It does not loop over pages, decode messages,
create runtime items or scan the filesystem. Aggregate and page queries use the
same resource projection and filters.

SQL describes committed registry state. Coalesced checkpoints can lag an active
transfer, and external deletion or modification is detected when storage is
accessed. Listing and counting never download content, pin files or refresh TTL.
`changed()` covers history membership, cache and range updates and may concern
other chats in the profile. Consumers should coalesce refreshes during transfers.
A response's `stale` flag reports invalidation while it was pending. Callbacks
run on the browser's thread and distinguish failure from empty results with their
boolean argument. Destroying the browser cancels delivery; the account and
manager also bound its lifetime.

Message provenance is optional. `requestMessages(resourceCursor, afterEvent,
limit, callback)` returns distinct logical message IDs, timestamps and short
previews for a resource, with `next`, `hasMore` and `stale`. It does not reconstruct
announcements. This supports a future explicit "Go to message" action without
making ordinary resource navigation depend on messages.

## Opening content

Resolve the selected entry with `manager->chatMediaItem(entry, account)`. The
borrowed item comes from the same registry and `fromShare` path as chat content.
Then use the common asynchronous opening contract:

```cpp
auto browser = manager->chatMedia(account, historyKey, gallery);
browser->setFilter({MediaFilter::Images, MediaFilter::Complete});
browser->requestPage({}, false, 40, [manager, account, gallery](ChatMedia::Page page, bool ok) {
    if (!ok || page.entries.isEmpty())
        return;
    auto item = manager->chatMediaItem(page.entries.first(), account);
    manager->open(item, gallery, [](FileReader *reader, MediaChunkError error) {
        if (!error.isEmpty())
            return;
        // The reader is open and parented to gallery. Pass it to a consumer.
    });
});
```

`open(item, owner, callback, maximumBytes)` returns a cancellation function.
Completion is queued, supplies an open `FileReader` or a typed `MediaChunkError`,
and is delivered once while the owner lives. Destroying the owner cancels its
pending operation. Explicit cancellation reports `Cancelled`; cancelling one
consumer preserves other consumers. The reader belongs to the owner after
success. Consumers must stop their decoder before destroying it; `abort()` wakes
blocked reads. The optional byte limit bounds automatic image preparation and
applies to unknown-length HTTP responses too.

A complete local file opens directly. Known-size, hashed partial resources use
the shared scheduler: reads request missing ranges and never add a full-download
goal. Address-only or unknown-length preparation uses one shared, spooled whole
transfer, verifies every announced hash, and learns the URL's content identity.
HTTP(S) remains supported. The player opener preserves existing HTTP URL streaming
for address-only media and backend compatibility adapters.

`aesgcm:` URLs follow [XEP-0454](https://xmpp.org/extensions/xep-0454.html): the key
and IV stay in the local fragment, HTTPS receives the fragment-free URL, and AES
GCM authentication finishes before any plaintext becomes available. This format
has one authentication tag for the whole file; first playback must therefore
wait for the complete encrypted object. Preparation is asynchronous, cancellable
and spooled to disk, not accumulated in memory. Failed authentication publishes
no plaintext file. This does not impose whole-file preparation on ordinary
hashed HTTP or Jingle range playback.

`download(item)->downloadAll(...)` adds an independently cancellable full-download
goal for a known-size resource; Save As uses that goal or the same asynchronous
open path before the bounded export job. Successful access and newly downloaded
bytes renew incomplete-file retention, while an active reader or download goal
pins the file. See [file readers and download goals](file-reader-download.md) for
scheduling, sparse storage, cancellation and backend lifetime details.

## Storage ownership and migration

History owns conversation membership and logical identities. `message_media_refs`
retains occurrence publication IDs; normalized resources hold names, MIME types,
sizes and hashes. `media_assets` and account-scoped `media_asset_aliases` associate
compatible announcements independently of their messages. These records are
maintained in the message transaction and orphaned assets are released with their
last reference. The file registry owns physical copies, verified hashes, ranges
and URL-to-content mappings. The existing history worker attaches it read-only.
Queries first restrict conversation membership using the contact index, then
resolve indexed hash, range and URL identities and aggregate resource membership.
No mutable statistics counter or separate synchronization service is introduced.

The unreleased history resource migration is extended in place; backfilling
preserves publication IDs, ordinals and scan positions. Protocol reconstruction
stays on the GUI thread; workers consume value projections. File schema 7 adds
an indexed MIME projection with a metadata backfill and an account/content index
for URL equivalence lookups. Existing file and range identities remain valid.

`MediaViewer` is the shared image/video window. Its small presentation source
uses this API for filtering and neighbors, decodes images off the GUI thread,
and reuses qite's video player. It retains zoom on metadata-only refreshes,
shows preparation/buffering state, skips immediately unavailable content and
keeps transient errors retryable. The window supplies rotation, copy, Save As
and fullscreen actions without implementing another downloader.
