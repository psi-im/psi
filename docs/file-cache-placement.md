# File cache placement and retirement

This is the placement and retirement part of stage 4 of the
[shared-file design](local-message-deletion-design.md). It extends `FileCache`
and its existing complete-file API. The subsequent
[SQL range checkpoints](file-cache-ranges.md) extend the same registry and
retirement protocol to range payloads.

## Ownership

A managed file has a random record ID and a unique payload name. Appending or
moving a replacement creates a new generation, even when its digest is the same.
No new payload overwrites a hash-named file. A delayed removal can therefore
address only the retired generation. Existing managed payloads keep their paths
when their old SQLite rows are upgraded.

An external file is recorded with `trackExternal`, an absolute path, its actual
size and modification time. Its bytes remain owned by the user or, for a legacy
complete-item view, by the range backend. External views are excluded from the complete cache's disk quota,
and deleting their records never unlinks their paths. A changed size, modification
time, missing file or replaced symlink invalidates the record. Tracking a file
already managed by this cache retains that ownership instead of deleting the
payload and treating its old path as an external original.

`FileCacheItem::filePath` resolves either placement. The older `fileName` accessor
is a relative managed payload name. File-sharing callers use `filePath`; they no
longer interpret an opaque `link` metadata field or construct a path themselves.
The display name is saved separately from physical placement, so the generated
payload name is not advertised as the sender's filename.

## Commit order

Retiring a managed file first commits a transaction that queues its relative
payload path in `file_trash` and removes its file/hash records. Only then can the
payload be unlinked. If retirement fails, the record, in-memory item and bytes
remain. If unlinking fails, the file stays unreachable through the registry and
the journal retries it on a later sync or restart.

Trash processing accepts only a single relative path component and checks that
no live record owns it. It never follows a stored absolute path for deletion.
Retirement inside a cache sync or legacy import waits for that outer transaction
to commit before processing trash. A new generation never reuses a retired path.

Payload writes use `QSaveFile`. A failed write does not mark the item on disk or
registered, and unloading keeps the only in-memory copy. Registration is marked
successful only after the SQL transaction succeeds; a failed batch retries after
a delay, preserving dirty metadata until it commits.
Empty managed files now have a real zero-byte payload instead of using the same
representation as a metadata-only external link.

Before recording a serving publication that names a managed payload, the
manager calls `FileCache::persist` for that item. Its ownership commits before
the catalog's publication transaction, even if the normal cache sync is still
pending. This barrier does not run quota eviction of the payload being published.

Sync and quota accounting visit each item once, irrespective of its hash aliases.
This also avoids deleting an item while traversing a hash-map iterator that still
contains its other aliases. A cache hit renews its lifetime only after half of
that lifetime has elapsed. Reloading a payload larger than the configured memory
limit returns its bytes without retaining another copy in the cache.
Retention starts when a moved file enters the cache, independently of the
original's modification time, which stays intact on the payload.

## Migration

File database schema 3 adds explicit placement, path and external modification
time to `files`, and adds the retirement journal. A complete legacy managed row
derives its existing hash-named path once; the upgrade records it without moving
or rehashing the payload. XML imports copy to fresh generation paths and record
ownership before retiring an interrupted old destination copy. Other profiles'
legacy input directories keep their existing import gate.

This file-database upgrade is forward-only. Older builds leave the newer database
closed and use their existing session-only cache fallback; they do not downgrade
or rewrite its records. The history database schema is unchanged by this step.

An old `link` has no recorded size or modification time. Migration cannot prove
that today's original still matches its old digest, so it does not create a fresh
trusted stamp from the current filesystem. The original is preserved. An explicit
serving publication can rebind it through the catalog's independently saved size
and modification time. Otherwise the old cache association is discarded and a
later request can obtain the file from its announced sources.

## Remaining stage 4 work

The range worker now uses the common SQL registry, retirement journal and disk
budget. Unreleased JSON sidecars are ignored rather than imported. A common RAM
tier, transport-completion evidence and large-file placement remain access-layer work.
The current complete-file operations still run through the legacy synchronous
API. Leases, bounded runtime items and a single reader/download path belong to
stage 5; optional message-triggered byte reclamation belongs to stage 6.

The regression coverage exercises external originals across quota enforcement and
restart, generation replacement, aliases, write/SQL failures, deferred unlink,
trash path validation, legacy SQLite/XML migration, damaged payloads and TTL
renewal. Existing avatar, BoB, publication and application tests remain the
compatibility boundary.
