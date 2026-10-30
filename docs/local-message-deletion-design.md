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

## Storage and transfer: strict separation of responsibilities

**Decision:** there is one durable *file cache* — FileCache. Download management, streaming and resumable staging are **not** caching and must not live in the same class. Remove MediaChunkCache as an independent persistent cache; do **not** merely rename it.

Responsibilities:

| Component | Responsible for | Must not do |
| --- | --- | --- |
| FileCache | Own verified completed payloads, checksum aliases, canonical file locations, metadata, retention, disk quota and safe GC | Schedule network requests, track active downloads or maintain a second range cache |
| FileSharingStore | Resolve account-specific SIMS/OOB descriptors, sources and verified URL-to-content mappings | Own another payload copy or decide physical deletion |
| MediaDownloadController (new) | Select source, coalesce in-flight requests for the same ranges, enforce limits/priorities, retry, cancel, prefetch and notify consumers | Register finished payloads or decide filesystem lifecycle and cache eviction |
| PartialDownloadStore (new) | Own exactly one staging file and an atomic sidecar containing which byte ranges have been received/validated. Serve available ranges through bounded background I/O | Keep a second registry of **completed** files, duplicate FileCache metadata or implement network scheduling |
| MediaStreamReader | Expose asynchronous QIODevice-style reads/seeks with backpressure and per-reader cancellation | Persist content metadata or control its lifetime in FileCache |
| MediaCleanupCoordinator (new) | Consult message-to-media reference ownership and in-flight readers, then request safe deletion of unreferenced managed content | Delete arbitrary paths or parse/own file data |

A logical download may move through these states:

```text
FileSharingStore descriptor
          |
          v
MediaDownloadController -----> MediaStreamReader (reads available ranges)
          |
          v
PartialDownloadStore: data.part + ranges sidecar
          |
          | complete, validated, transfer finalized
          v
FileCache: canonical payload + verified hashes / aliases
          |
          v
MediaCleanupCoordinator: remove only after all references are gone
```

### Partial staging and transition to FileCache

1. While downloading, keep the payload in a staging file and record *only transfer-specific state* (received/verified intervals, expected length/identity, error and resumption state) in an atomically written sidecar. Refer to checksum/source metadata through the existing file metadata catalog; avoid copying entire SIMS descriptors or URL lists into the sidecar. All range writes and validation run on a worker thread.
2. A sidecar's presence does **not** currently distinguish incomplete files: today's MediaChunkStore persists JSON for complete files too and creates its data file before any download. Establish a new unambiguous `.part`/sidecar lifecycle for unfinished downloads only. On startup, verify the sidecar, file length and recorded ranges; do not confuse a sparse file's size with downloaded bytes.
3. Once complete and **verified**, adopt the staged file into FileCache by a same-filesystem move where possible, preserve hash aliases and metadata, then remove the staging sidecar **after** the cache registry has been committed. Make the transition crash-recoverable and idempotent. For checksum-less OOB, compute SHA-256 from actual bytes before registration.
4. Do not serve a partial staging file as an ordinary completed FileCache item. Stream only recorded valid spans. Preserve HTTP/Jingle range integrity, exact-read semantics, checksum mismatch handling, resume and transport-completion checks. An interrupted download must never be presented as a verified complete file.
5. Respect active readers/publishers when transitioning files. On Windows an open file may not be renameable: drain/cancel readers or use a managed promotion strategy rather than silently breaking playback.
6. Extend FileCache with **file-backed bounded reads**, not FileCacheItem::data() for large files (which presently reads the entire file into memory). Keep decoding, range I/O and checksum verification off the GUI thread.
7. Unify quota accounting. Today FileCache defaults to 50 MiB and MediaChunkCache to 256 MiB; consolidating under FileCache must not unexpectedly evict large media. Continue honoring pinned published files and explicitly retained partial downloads without double counting aliases.
8. Migrate existing `shares/media-files-v2` data safely: completed verified files become FileCache items, unfinished files become staging entries, and existing FileCache external links/published files remain valid. Keep a read-only migration path until verified. **Never delete the old directory just because the new code builds.**

### Media cleanup and ownership

The durable message-media ownership ledger lives with history, independently of transfer state. FileCache exclusively removes completed *managed* files; PartialDownloadStore removes incomplete staging and its sidecar. MediaDownloadController must cancel or drain requests and release readers before physical cleanup. Check references across all accounts and chats, hashes/aliases, and retained/published items. Do not unlink an arbitrary FileSharingStore localPath or a file the user saved with Save As.

This leaves **one persistent completed-file cache** and **one temporary incomplete-download store**. The controller may use MediaChunkScheduler internally, but it may not become another cache.

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
3. For incomplete downloads, add asynchronous PartialDownloadStore cleanup: cancel or await active readers, respect pins and remove the managed staging file plus its sidecar. Finished media is removed through FileCache only.
4. Remove stale FileSharingStore descriptors and URL mappings only when unreferenced. **Never delete externally owned localPath**, such as a user Downloads file. Never trust an arbitrary path from a message.
5. Retry transient file sharing violations after restart; deletion failure must not resurrect a message.

Default UX: Delete locally (no file cleanup), plus a deliberate checkbox 'Also delete cached attachments that are not used elsewhere'. Later provide a cache management view and reclaimable-bytes summary.

## Test plan and implementation stages

1. SQLite schema migration; atomic tombstones/scrub across corrections; no-resurrection on stable/unstable/live MAM and restart; indexed page geometry and local history/search filtering.
2. Native MAM menu and multi-selection UI, with persistence-first row removal; legacy UI later. Ensure this does not send XEP-0424.
3. Media ledger on live, MAM and verified downloads, plus incremental backfill and account-scoped cross-message ownership tests.
4. Strictly split MediaDownloadController (requests, retries, scheduling), PartialDownloadStore (unfinished payload and verified ranges), MediaStreamReader (bounded asynchronous reads), and FileCache (the only finished-file cache). Migrate old media-files-v2 safely; add one durable GC queue with reader/pin protections and optional attachment cleanup UI.
5. Integration tests for duplicate IDs, correction-before-original, cross-account collisions, sparse MAM, OOB URL mapping, one blob referenced twice, external localPath, concurrent streaming and interrupted transactions.

Do not ship Delete locally until stage 1 is complete. Do not offer cached-file deletion until ownership and GC are verified.
