# SQL range checkpoints

Range payloads now use the profile's `files.sqlite`, alongside complete cached
files and publications. `FileCache` owns the range backend and the shared-file
disk budget. `MediaChunkStore` remains a transitional range-I/O adapter with a
private worker; `MediaChunkCache` keeps the existing reader/scheduler interface.
There is no JSON persistence or sidecar discovery.

## Records and ownership

Schema 4 adds an immutable resource key, storage-completion flag, payload
modification stamp and integrity error to `files`. `file_ranges` records available
spans, and `file_checksums` records required digests and their verified status.
Both child tables cascade with the owning generation. Payload bytes remain in
ordinary files, with fresh UUID names under the profile's `shares/ranges`.
Replacing an evicted resource never reuses its old generation's path.

Each complete-cache partition has a range subpartition, such as `shares:ranges`,
with its own payload root. This keeps relative retirement paths unambiguous.
Both use the same metadata encoding, schema and commit-before-unlink helpers in
`FileDatabase`. The worker opens and removes its own SQL connection on its own
thread; it receives the database path before the thread starts.

The complete-item API can retain a non-owning, stamped view of a completed range
payload for legacy callers. That view does not own or charge its bytes again.
`FileSharingManager` checks range availability before using such a path, including
when restoring a serving publication. Publication authorization still belongs to
the catalog.

## Checkpoint order

1. Register the generation before creating its payload. A crash before the first
   coverage checkpoint leaves a cache miss.
2. Accumulate new coverage and checksum results in memory. Coalesce partial-file
   checkpoints to one per five seconds per file. Configuration changes, completion,
   explicit checkpoints and shutdown force pending state to disk. Unchanged cache
   hits neither rewrite SQL metadata nor repeat a verified checksum.
3. Before overwriting any checkpointed span, commit its withdrawal and invalidate
   overlapping verified results. If that transaction fails, do not write the bytes.
4. Synchronize changed payload bytes before committing their coverage, checksum
   results and completion state together. Failed checkpoints retain dirty state
   and retry; they do not advertise a successful durable completion.
5. Retire a generation and queue its managed name in `file_trash` in one
   transaction. Unlink afterward and retry failures at checkpoint/startup. Trash
   processing validates names and checks for live owners.

The worker uses SQLite WAL with `synchronous=FULL` and native payload synchronization
(`fsync`, macOS full synchronization where available, or `FlushFileBuffers`).
`QFile::flush` alone is insufficient for this ordering; see the
[Qt file API](https://doc.qt.io/qt-6/qfiledevice.html#flush) and
[SQLite synchronization modes](https://www.sqlite.org/pragma.html#pragma_synchronous).
A lost checkpoint can require downloading a tail again. Its bytes never become
available merely because a sparse file has that logical length.

## Startup and limits

Startup loads bounded range/checksum metadata and checks payload paths, existence,
length and modification stamps. It does not hash payloads. Missing or truncated
files withdraw affected coverage. A changed stamp withdraws verified results;
checksums are evaluated on demand before returning stored bytes. Unchanged
verified results survive a restart.

The shared-file disk budget is 256 MiB for complete and range data together.
`FileCache` accounts for range bytes while sweeping complete items, then gives
the worker the remaining allowance. External originals and non-owning aliases
are excluded. Existing pin, publication and retained-partial protections remain;
as before, the most recent sole range resource can exceed the allowance. This is
a soft cache budget, not a limit on transfer size. The RAM tiers are still separate.

The worker bounds resident resources, fragmented coverage and checksum rows.
The storage-completion flag retains the existing adapter's meaning: full coverage
and required checksums. Transport-success evidence, source attribution, leases,
one RAM tier and the common reader/download API remain access-layer work.

## Compatibility

Old `media-files-v2` JSON and payloads are deliberately ignored. They came from
unreleased builds and are refetchable; there is no importer, directory adoption,
sidecar sweep or trust in a hash-named old payload. Existing retirement of the
whole shared legacy directory keeps its normal import gate. New generations cannot
overwrite them. Older serving records naming those paths remain unavailable
until the content is fetched again.

Existing complete SQLite records upgrade without moving their payloads. Legacy
XML imports for complete caches remain supported. The file-database schema
upgrade remains forward-only.

Tests cover real process termination before a checkpoint, committed withdrawal
before overwrite, SQL failure and automatic retry, deferred unlink and generation
replacement, unchanged-hit SQL/hash avoidance, shared disk accounting, ignored
legacy sidecars, damaged payloads and existing reader/scheduler behavior.
