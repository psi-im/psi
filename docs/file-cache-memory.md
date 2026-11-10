# Shared file-cache memory

Each `FileCache` partition now owns one byte-bounded LRU tier for small complete
files and recently read ranges. Shared files use 16 MiB; avatar and BoB partitions
retain their existing defaults. The disk budget and SQL schema are unchanged.

## Ownership and keys

`FileCacheMemory` is an owner-thread implementation detail, with no SQL, file I/O,
protocol state or callbacks. Keys contain a managed generation ID and a byte span.
Hash aliases share the same generation. A complete-item view of a managed range
payload uses that payload's generation, so both APIs can reuse the same bytes.
External originals use their complete-item record IDs and existing stamp checks.

One cached span can satisfy a contained read. Overlapping admissions replace old
spans conservatively, and retirement or integrity invalidation removes the affected
generation. LRU eviction removes its span index as well as the payload reference;
it does not retain a history of evicted keys. The index is capped at 8192 spans,
including when payloads are only a byte long. Limit changes apply immediately to
both interfaces. Consumer buffers remain independently bounded by their readers.

`FileCacheItem` no longer retains successfully stored data itself. It keeps only
uncommitted staging bytes until an atomic payload write succeeds, then offers them
to the shared tier. Failed writes preserve the only copy even when the RAM budget
is zero. This staging exception is necessary for the legacy synchronous append API;
it is not a second read cache. Oversized complete reads return their bytes without
retaining a cache copy.

## Range reads

The range-store facade uses the same tier on its owner thread. The disk worker
never touches the RAM data structure. Successful worker reads carry their actual
generation ID, and are admitted only while that generation and coverage remain
current. Pending new checksum requirements, writes, retry or invalidation prevent a RAM read
from bypassing the queued worker operation. Repeating known requirements or updating
publication metadata keeps hot bytes available.

RAM hits complete asynchronously. Their bytes are looked up when the callback is
ready to execute, rather than copying a slice at admission. If the tier was evicted
or its availability changed in the meantime, the read goes through the worker.
Cancellation of a chunk request still settles its callback once. A hot RAM read
bypasses the network scheduler's active-fetch limit.

`MediaChunkCache` no longer owns a `QCache` or a per-chunk `records_` map. It keeps
the existing demand scheduler and its bounded requests; available ranges come
directly from the range-store state. Cached bytes do not create independent claims
of coverage. This reduces the responsibilities that need to move to `FileDownload`.

The backend is owned by `FileCache`, whose teardown closes it before destroying
the shared tier. A chunk adapter borrowing that backend must be destroyed first,
as `FileSharingManager` already ensures.

## Remaining access-layer work

This completes the RAM ownership part of stage 4. It does not introduce a parallel
reader or transport stack. `MediaChunkReader` and the current download adapters
remain the consumers until the common `FileReader`/`FileDownload` contract replaces
them. Transport-finalization evidence, unknown-size resources, leases and bounded
`FileSharingItem` lifetimes remain stage 5 work.

Regression coverage checks cross-interface pressure, alias reuse, immediate
shrinking, safe retry after an evicted queued hit, generation isolation, byte/LRU
limits, index churn, and invalidation/overwrite/new checksums before a queued hit
is delivered. It also checks a hot read with all network fetch slots occupied.
