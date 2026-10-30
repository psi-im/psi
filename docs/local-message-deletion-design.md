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

## Media file lifecycle: register at first sight, one FileCache for partial and complete files

**Architecture decision:** FileCache registers and owns a file **immediately**, before any chunks have arrived. The presence of a managed, per-file *incomplete-download sidecar* means that byte ranges must be checked before a read. When there is no sidecar, FileCache may serve the entire file, **but only if** its registry has a durable, verified-complete state. Removing or losing a sidecar must never accidentally upgrade an unfinished file to a complete one. This supersedes the earlier "stage first, then promote/register into FileCache" design.

The existing MediaChunkStore already persists a sidecar with downloaded ranges via QSaveFile, but it currently writes one after every chunk and keeps a sidecar even for complete files. Its durable tracking and verification code should migrate inside **FileCache** (possibly with a private disk-I/O worker). Eliminate its *second* disk cache and completed-file registry; do not create another persistent PartialDownloadStore competing with FileCache.

### Exact ownership boundaries

| Component | Responsibility |
| --- | --- |
| **FileCache** | Register partial or complete files; own the single canonical managed payload, size, checksum aliases/metadata, current available/validated ranges, sidecar and quota; answer range reads and explicit cache misses; persist chunk data and compact sidecar; verify full file; GC complete and partial files safely |
| **FileShareTransport** (rename FileShareDownloader) | HTTP/FTP, Jingle FT and BoB transport, per-source failure fallback, actual Content-Range/size checks, bounded network QIODevice and transport success/failure. **Never writes to disk** |
| **MediaChunkScheduler** | Deduplicate demand for missing ranges, prioritize reads, limit concurrent transfers and cancel waiters |
| **MediaChunkTransfer** | Validate boundaries, exact byte counts and transport completion before committing fetched data |
| **MediaChunkReader** (later rename if needed) | Consumer-facing asynchronous QIODevice with bounded buffering, lookahead, cancellation and backpressure |
| **FileSharingStore / FileSharingItem** | Source and descriptor metadata, account+URL resolution and the file identity needed to find/register the corresponding FileCache item; no payload ownership |

**FileSharingChunkSource** should not survive as a separate manager. Today it combines identity/configure/metadata lookup, source-health selection, Jingle session-pool hookup, transfer validation and fallback to a shared sequential transfer when Range is unsupported. Redistribute these pieces to FileSharingItem/Store (identity/descriptor), FileShareTransport (transport/source selection and pool), scheduler/chunk request logic (shared sequential demand), and FileCache (available ranges/metadata/storage). Remove it only when the tests cover Jingle verification, source fallback, sequential non-Range HTTP, cancellation and multiple media readers.

### Five-party read/download sequence

The lifelines intentionally reflect responsibilities, **not** a chain of classes. The "chunk entities" participant includes MediaChunkScheduler, MediaChunkReader and transfer validation, but no persistent payload storage.

```mermaid
sequenceDiagram
    participant R as Remote party
    participant T as Local party transport
    participant P as Local party media player
    participant C as Local party chunk entities
    participant F as FileCache

    Note over F: Register canonical entry on first reference, BEFORE downloading
    P->>C: read(offset, length)
    C->>F: readRange(fileId, offset, length)
    alt Complete cache hit
        F-->>C: bytes / managed complete-file handle
        C-->>P: bytes
    else Partial hit / cache miss
        F-->>C: available bytes + exact missing ranges
        C-->>P: already available bytes
        C->>C: schedule/deduplicate missing ranges
        C->>T: fetch(missing offset, length, sources)
        T->>R: HTTP Range / Jingle FT / BoB
        R-->>T: data + protocol result
        T-->>C: bounded bytes + actual response metadata
        C->>C: validate range, transfer and known checksums
        C->>F: writeAcceptedRange(fileId, offset, bytes)
        F->>F: write data; update RAM range map
        F-->>C: range now available
        C-->>P: deliver requested bytes
        opt Batched checkpoint
            F->>F: atomic sidecar flush, throttled to ~5s
        end
        opt Full coverage, final hash and transport validation
            C->>F: finalize(fileId)
            F->>F: sync payload and complete registry; remove sidecar
            F-->>C: complete in FileCache (same registered entry)
        end
    end
```

The FileCache item is **not re-registered or moved into a second cache at completion**. Only its state changes from Partial to Complete and the sidecar disappears. On a complete-file hit, FileSharingDeviceOpener may pass the canonical FileCache path directly to the player. On a partial hit, the player must get only bytes FileCache marks available, never the raw sparse file path without range validation.

### Sidecar format, in-memory updates and 5-second checkpointing

- FileCache keeps the authoritative **live range map in memory**, updated after every successfully persisted/accepted chunk, and reports cache hits and misses directly to callers. The existing per-file metadata sidecar (migrated from MediaChunkStore) records the durable available ranges, expected file size, partial/validation state and whatever checksum information is needed to recover safely.
- **At most one ordinary sidecar write per ~5 seconds per dirty file**, using a coalesced timer and atomic replacement (QSaveFile). This reduces sidecar rewrite frequency. Force a checkpoint on normal close/shutdown, explicit pause and before finalizing completion. If updates arrive continuously, the timer still flushes periodically; do not postpone them indefinitely.
- Disk/sidecar ordering is essential: write and flush chunk data *before* advertising it as durable in the sidecar. A crash may lose up to the most recent uncheckpointed range-map updates, resulting in harmless re-fetch of already written bytes; it must **never** mark unwritten bytes as available. On restart, load only persisted ranges and check payload size and integrity.
- **Overwrites and invalidations are exceptional and must be durable before mutating bytes.** If an already persisted range is to be overwritten, withdrawn or invalidated because of checksum/transport failure, flush a sidecar checkpoint that first removes that range, *then* change file contents. Otherwise a crash could restore obsolete sidecar coverage over invalid bytes. New, nonoverlapping chunk writes can use the ordinary 5-second batching.
- Completion is a synchronous durability boundary on the storage worker: full range coverage plus required hashes and transport completion (including Jingle receipts) must be checked; the completed FileCache registry state and payload must be committed before removing the sidecar. Recovery treats any missing-sidecar file without a durable Complete registry marker as **unverified/incomplete**, not as a full cache hit. Never invent valid bytes by examining sparse file length alone.
- Some protocols only provide a **whole-file checksum**, not a cryptographic hash for each chunk. Distinguish "received and bounds/transport-validated" from "content-hash-verified" ranges in API/state; a player may stream accepted bytes according to policy, while full trusted completion still requires the final digest. An integrity failure invalidates the affected range or whole file as appropriate.
- Registry insertion itself must be durable before the first chunk becomes visible and must support an account-scoped *provisional identity* if no content hash is available (e.g. hashless OOB). After downloading, compute the real content SHA-256, register verified aliases and merge the provisional entry without losing active readers. Never pretend an OOB URL hash is a content digest.
- Expose a range-capable API conceptually equivalent to `readRange(fileId, offset, length) -> {availableBytes, missingRanges, state}` plus change notifications. The cache does **not** initiate HTTP/Jingle requests; its callers schedule only exact missing ranges via MediaChunkScheduler / FileShareTransport. The API must also accommodate zero-copy/bounded file-backed reads without calling FileCacheItem::data() on large media.
- Account for in-flight readers/pins and completion GC safely; unify existing 50-MiB FileCache and 256-MiB MediaChunkCache quotas without unexpected eviction. Managed payloads and sidecars both belong to FileCache; user-exported files referenced from metadata must never be deleted.
- Migrate old `shares/media-files-v2` entries (including complete files with JSON sidecars) safely. Only after verifying the persisted data and associating identities should FileCache adopt them. Preserve partial data/ranges and account-scoped source identities across restarts; do not remove the old directory blindly.

### Why there is no separate PartialDownloadStore

A private storage worker for asynchronous file writes and sidecar checkpointing is an implementation detail of FileCache, not a second logical owner of partial media. It may retain the verified-range algorithms from MediaChunkStore and continue to use its existing test cases, but FileCache is the only public interface for partial/full lookups, byte availability, aliases, retention and deletion. This also makes history-driven cache reclamation a single ownership operation.

### Media cleanup

The durable message-media reference ledger lives with history, independently of downloading. On local message deletion, collect candidate identities **before** scrubbing the message, and enqueue candidates transactionally. After commit, GC checks ownership across all accounts/chats, active readers, published items and in-flight downloads before asking FileCache to remove its managed payload and sidecar **together**. A removal must never unlink an arbitrary external FileSharingStore localPath, user's Save As copy or file held by another message.

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
3. Use FileCache for both complete and incomplete managed entries. Before removal, cancel or drain transport/stream readers, respect pins and remove the canonical payload plus any incomplete-download sidecar together.
4. Remove stale FileSharingStore descriptors and URL mappings only when unreferenced. **Never delete externally owned localPath**, such as a user Downloads file. Never trust an arbitrary path from a message.
5. Retry transient file sharing violations after restart; deletion failure must not resurrect a message.

Default UX: Delete locally (no file cleanup), plus a deliberate checkbox 'Also delete cached attachments that are not used elsewhere'. Later provide a cache management view and reclaimable-bytes summary.

## Test plan and implementation stages

1. SQLite schema migration; atomic tombstones/scrub across corrections; no-resurrection on stable/unstable/live MAM and restart; indexed page geometry and local history/search filtering.
2. Native MAM menu and multi-selection UI, with persistence-first row removal; legacy UI later. Ensure this does not send XEP-0424.
3. Media ledger on live, MAM and verified downloads, plus incremental backfill and account-scoped cross-message ownership tests.
4. Extend FileCache to register files immediately and own partial/complete range state, disk worker and a coalesced ~5-second sidecar checkpoint. Remove the second MediaChunkStore cache; rename FileShareDownloader to transport-only FileShareTransport; retain MediaChunkScheduler and MediaChunkTransfer, redistribute and remove FileSharingChunkSource. Preserve sequential non-Range fallback and Jingle success sequencing; migrate old media-files-v2 safely and test cache-miss reads, restart recovery, concurrent readers and GC.
5. Integration tests for duplicate IDs, correction-before-original, cross-account collisions, sparse MAM, OOB URL mapping, one blob referenced twice, external localPath, concurrent streaming and interrupted transactions.

Do not ship Delete locally until stage 1 is complete. Do not offer cached-file deletion until ownership and GC are verified.
