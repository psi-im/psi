# Local-only message deletion and safe media cleanup

Status: **design, not implemented**. Initial scope: 1:1 chats backed by SQLite/MAM. This is separate from XEP-0424 (remote retraction).

## User contract

- Delete locally: permanently hide a *logical* message (including corrections) on this device, without transmitting an XMPP stanza.
- The message must stay hidden after reconnect, restart, live echo, MAM replay, indexed seek, and local history search.
- Optionally delete locally **and reclaim unshared cached media**. Never promise to delete remote MAM, another client's copies, server-hosted media, backups, or files exported with Save As.
- No optimistic removal until persistence commits. No UI action when message identity cannot be established reliably.

## Existing storage and hazards

- EDB stores text, subject and message extensions in events; archive_items.xml retains the *full* MAM forwarded stanza. Both contain content, SIMS/OOB references and possibly decrypted information. Clearing only m_text is not deletion.
- Corrections are indexed in message_revisions; state and receipts in message_state_refs. Archive pages and indexed islands depend on event IDs, archive UIDs, neighbours, and item positions.
- **Current duplication (to be removed):** FileCache owns verified small payloads and their hash aliases under shares/cache.xml, while MediaChunkCache/MediaChunkStore owns a second file registry, disk quota and JSON sidecars under shares/media-files-v2. Some completed/published files appear in both registries. FileSharingStore separately persists account-scoped SIMS descriptors and OOB URL-to-verified-content mappings, sometimes with external localPath pointers.
- A single payload hash/source may have references in multiple messages, chats and accounts.

## Tombstone and scrubbing

Introduce an account/chat scoped, identity-only tombstone table:
- account, bare peer JID, direction; trusted MAM archive service+UID and client message-ID/origin-ID aliases; marker that the logical message was deleted locally.
- Distinguish trusted archive IDs from user-generated IDs, and reject ambiguous IDs. Unavoidable conversation scope and archive ordering metadata may remain; no body, URLs, media hashes, captions, HTML, filenames or raw stanza in tombstones.
- Mark the logical root and *all* related corrections with a new LocalDeleted bit separate from the existing Displayed/Delivered/Edited/Retracted bits. Keep only the structural event and revision identifiers needed for MAM ordering, identity checks and pagination.
- In **one SQLite transaction**, insert tombstones, clear events.m_text/subject/lang/extra_data/resource as appropriate, scrub every archive_items.xml for the logical message, and enqueue media cleanup candidates. Roll back all changes if any step fails.

### No resurrection

- Before live append or MAM decode, resolve account+peer+direction+UID and safe ID aliases. Suppressed messages must not get decrypted, reinserted, or remembered as file shares.
- HistoryArchive::storePage must preserve tombstones even when receiving a newer stable MAM page. A newly arrived correction attached to a deleted root must also be suppressed and scrubbed.
- Cached pages cannot treat deliberately scrubbed XML as corruption requiring endless network refetch. Preserve the MAM page's original event count/UID positions and represent suppressed entries as *invisible placeholders* rather than inventing a visible fake message.
- Filter tombstones in nativeArchivePage, prependLocalChatHistory, resolvedHistoryEvent, the classic history dialog, and search, while preserving the cursor and indexed island coordinates.
- The local-only action must never send XEP-0424.

SQLite UPDATE/DELETE does not ensure secure erasure of old page, journal, WAL, snapshot, or backup bytes. Evaluate PRAGMA secure_delete and a controlled WAL checkpoint/VACUUM in a separate cleanup operation. Do not claim physical secure erase from SSD or backups.

## Unified cache target: FileCache is the only owner of completed media

**Decision:** eliminate MediaChunkCache **as a second persistent file cache**, but keep its streaming responsibilities during the refactor. FileCache remains authoritative for verified completed payloads, hash aliases, metadata, retention, file ownership and garbage collection. FileSharingStore remains the *descriptor and source* catalog, not a payload cache.

The current MediaChunkCache is **not just a cache**: it wraps MediaChunkScheduler (deduplicated/cancellable range requests and limits), MediaChunkReader (64-KiB consumer buffer and bounded prefetch), and MediaChunkStore's worker (off-thread sparse writes, crash-safe range accounting, hash verification, retry and pinning). Preserve these properties in a renamed FileSharingStreamManager/PartialDownloadStore (names provisional), not another content registry.

### Partial file staging

1. Until the entire content is verified, write to one managed staging data file and an atomic sidecar containing completed/verified ranges, expected size, known checksums, and transient error/transfer state. Keep bounded reads and writes off the UI thread. Never expose an incomplete file via FileCache's ordinary content-hash lookup.
2. A sidecar's **presence alone does not indicate incomplete download in today's code**: MediaChunkStore currently writes a JSON sidecar even for fully completed files, and it creates a .data file on configuration before downloading any bytes. Change the *new format's* convention so a specific partial sidecar exists **only while staging**; complete state is declared only after verified coverage and successful promotion into FileCache.
3. On verified completion, atomically adopt/move the staged file into FileCache (ideally a cheap same-filesystem rename), record its hashes + aliases/metadata, and remove the partial sidecar only after the FileCache registry is durably committed. Handle crashes at each boundary idempotently; recover orphans on startup without double-registering or silently discarding bytes.
4. Continue validating actual available ranges and checksums before reading, especially after restart, checksum changes, truncated files, corruption or failed network transfers. A disk file's apparent length may contain holes or invalid bytes. For whole-file checksums, also verify that the transport finished successfully. Preserve HTTP/Jingle offset/range semantics.
5. Prevent overwriting or moving a staged file while active streaming readers or publishers hold it. Explicitly define pin counts and draining/cancellation; do not rely on Windows permitting renames of open files.
6. Make FileCache support **file-backed, streamed** payload access without calling FileCacheItem::data() on multi-gigabyte media (today that reads the whole file into RAM). Keep worker-thread I/O and a small bounded in-memory read-through cache. Existing FileCache::moveToCache and linked-metadata records are not sufficient as-is.
7. Unify disk quota/accounting. Currently FileCache defaults to 50 MiB and MediaChunkCache to 256 MiB: dropping the latter without revisiting the quota would regress large media storage. Keep explicitly protected published files and user-approved partial downloads, and avoid double-counting hash aliases.
8. Migrate persisted shares/media-files-v2 safely: scan sidecars, recover verified ranges/checksums, register completed payloads into FileCache, and leave incomplete downloads in staging. Treat references from old FileCache metadata links carefully. Retain read compatibility until successful migration; never just delete the old directory on startup.

### Ownership and cleanup

Track the live logical-message-to-content relationships independently of FileCache, in the history database. FileCache must then offer one safe removal path for **managed** completed payloads, and the staging store one removal path for unfinished payload/sidecar pairs. Cross-account reference checking is essential because FileCache keys content globally while streamed identities are account-scoped. Never unlink an arbitrary FileSharingStore localPath or the user's saved/downloaded file.

This consolidates final payload ownership into **one** FileCache rather than maintaining the two disk caches and inventing two parallel GC implementations. Scheduler/stream/read logic may keep separate classes for concurrency, but not a second persistent completed-file catalog.

## Durable media ownership ledger

Record references *as attachments are learned*, before the first message deletion:
- message_media_refs: logical event/root ID, media kind, canonical identity, expected file size; deduplicate identical references. For SIMS use verified content hash+algorithm; for OOB use account-scoped canonical URL identity until content is verified; for streamed media use FileSharingChunkSource's immutable account-scoped source identity + size.
- Reuse MessageAttachments::collect and FileSharingStore mappings; do not write another media parser.
- On message deletion, within the same transaction collect potentially orphaned managed-media identities, drop this message's reference edges, and insert durable media_cleanup_candidates entries. The tombstone itself retains **no** media hashes or URLs.
- Backfill old history references incrementally, in bounded batches. **Fail closed** until ownership coverage is complete: if an older message might still reference a blob, do not delete it.
- Deduplicate across message corrections, chats and accounts, and include user-published uploads, ongoing downloads and reader pins.

### Asynchronous garbage collection

After the history transaction commits, a worker consumes candidates and rechecks current ownership:
1. Skip shared or pinned media, with distinct retained-versus-failed status.
2. For FileCache, remove the canonical cache item through its API, not one arbitrary hash alias or filesystem path.
3. Add an explicit asynchronous MediaChunkStore erase(source,size) operation (currently absent): cancel or await active readers, respect pins, delete managed payload + validated sidecar, and update memory indices and availability signals on its own worker thread.
4. Remove stale FileSharingStore descriptors and URL mappings only when unreferenced. **Never delete externally owned localPath**, such as a user Downloads file. Never trust an arbitrary path from a message.
5. Retry transient file sharing violations after restart; deletion failure must not resurrect a message.

Default UX: Delete locally (no file cleanup), plus a deliberate checkbox 'Also delete cached attachments that are not used elsewhere'. Later provide a cache management view and reclaimable-bytes summary.

## Test plan and implementation stages

1. SQLite schema migration; atomic tombstones/scrub across corrections; no-resurrection on stable/unstable/live MAM and restart; indexed page geometry and local history/search filtering.
2. Native MAM menu and multi-selection UI, with persistence-first row removal; legacy UI later. Ensure this does not send XEP-0424.
3. Media ledger on live, MAM and verified downloads, plus incremental backfill and account-scoped cross-message ownership tests.
4. Replace the duplicate completed-file cache with FileCache-backed verified payloads + a partial-download staging worker. Migrate old media-files-v2 safely, then add one durable GC queue with reader/pin protections and optional attachment cleanup UI.
5. Integration tests for duplicate IDs, correction-before-original, cross-account collisions, sparse MAM, OOB URL mapping, one blob referenced twice, external localPath, concurrent streaming and interrupted transactions.

Do not ship Delete locally until stage 1 is complete. Do not offer cached-file deletion until ownership and GC are verified.
