# Shared media chunk cache

The local HTTP file-sharing proxy uses a shared range cache for remote audio and
video with a known nonzero size and content hash. Both WebEngine and external
players use this HTTP path. Qite's network access manager path and ordinary file
downloads retain their existing behavior. Complete files in the existing file
cache take precedence.

`MediaChunkScheduler` is adapted from
[AnyKeep PR #156](https://github.com/Ri0n/AnyKeep/pull/156), reviewed at commit
`db77d42ef791225839d03ecb0c62cd06462b31e3`. It provides shared in-flight requests,
independent reader cancellation, priorities, and bounded admission. Unused
AnyKeep owner-retirement, singleton and external WebDAV/Jingle accounting APIs
are deliberately omitted from the Psi adaptation. Psi does not
use AnyKeep's encrypted record format. `FileSharingChunkSource` delegates to
Psi's existing downloader and transports; it does not introduce a second Jingle
stack.

## Storage and loading

- Chunks are aligned to 4 MiB boundaries. At most two cache fetches run at once,
  also limited to two per account. These are logical cache-fetch slots, not a
  global count of physical Jingle sessions. Session teardown after cancellation
  can outlive the loader. This limits admission, not bandwidth.
- A new seek has priority over continued sequential reading. A reader requests
  more only as its consumer drains its 64 KiB buffer.
- Each missing chunk currently needs its own existing downloader/Jingle transfer.
  Transport reuse and adaptive bandwidth pacing are not implemented.
- The complete bounded transfer must succeed before its chunk is served. Jingle
  success includes the existing checksum/receipt processing. This means an initial
  read waits for a whole chunk, rather than streaming its first bytes immediately.
- Identity includes the account, content hash, advertised size, chunk size and
  index. Received size and range must agree with the request. Servers that
  ignore a bounded range fall back to the existing direct path before headers
  are sent. Wrong partial ranges fail instead of falling back. Typed error codes
  control this decision; there is no process-wide source blacklist.
- Upstream total size may be unknown for HTTP partial responses; an explicitly
  conflicting total is rejected. Exact range length and provider completion are
  still required.
- Files live in `shares/media-chunks-v1` under Psi's cache directory. Records are
  plaintext, atomically committed, and disposable. A record checksum detects local
  corruption; it does not authenticate remote content.
- The disk budget is 256 MiB and at most 2048 records, with oldest-use eviction.
  Across restarts, eviction age is approximated by write time. Memory holds at most
  16 MiB of cached payload, in addition to active transfers and reader buffers.
  Disk and memory eviction are independent. Disk reads, hashing, writes and
  pruning run on one worker thread; callbacks return to the application thread.
  An unavailable/full disk leaves memory caching usable. Invalid or incomplete
  records are cache misses.
- Canceling a reader detaches its waiter. A shared fetch survives while another
  reader needs it. The scheduler retains admission until an orphaned transfer has
  acknowledged cancellation. At most 256 read callbacks are retained. Overload
  rejects admission without queuing another callback.

HTTP Range behavior follows [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html#section-14),
including ignoring Range on HEAD and allowing a partial response with an unknown
complete length. Jingle ranges continue to use the existing
[XEP-0234 implementation](https://xmpp.org/extensions/xep-0234.html#range).

## Validation

Standalone tests in `tests/filesharing` build with Qt 5 or Qt 6. The scheduler and
cache tests cover overlapping reads, cancellation, admission, persistence,
corruption, quotas, overload, completion after owner destruction and shutdown.
`MediaChunkTransfer` tests exercise the production bounded-transfer state machine
with fake devices: success after the payload, integrity failure, missing/wrong
ranges, EOF, cancellation, and success/disconnection emitted inside the final
`readData()`. The Psi adapter only wires real downloader metadata and signals to
this object. All network protocol processing remains in the existing downloader.
Replaying the 67 AVI seek offsets from the reported mpv session uses three physical chunk fetches in
the test fixture; this is not a measurement of live Jingle sessions.

For a real playback check, rebuild the receiving Psi, open the same AVI externally,
seek in both directions, and repeat the request to verify reuse. The sender uses
the existing ranged-transfer protocol and does not require this cache change.
Enable `psi.filesharing.chunks.debug=true` in Qt logging rules to record cache
fetch offsets, lengths and completion errors. Verify that repeated nearby HTTP
ranges share chunks and no longer create a Jingle session per index entry.

## Architecture review

The intended ownership path is HTTP response → reader/waiter, while the manager
owns the cache/scheduler and active shared fetches. Destroying one response cancels
its waiter without destroying other consumers' transfers. The cache owns its disk
worker and drains outstanding disk work at teardown; the ordinary read path never
blocks on the worker. Cross-thread completions use a Qt signal relay whose
receiver connection is removed on destruction, rather than dereferencing a
`QPointer` from another thread.

The Iris read-hook change fixes callable lifetime at the connection abstraction:
removing registration cannot destroy an executing callback, and mutable captures
persist between invocations. It does not change FT signaling or bypass validation.
The existing FT layer and the bounded-transfer adapter both need final-read
reentrancy tests. Full consumers must be rebuilt with the changed Iris header.

Remaining tradeoffs are explicit: 4 MiB initial latency and read amplification,
per-chunk Jingle negotiation, and best-effort per-process disk quotas if multiple
processes share a cache directory. Transport reuse belongs in the downloader/Jingle
session lifecycle rather than in the disk store or HTTP widget. Initial streaming
before a chunk is verified would need a separate policy for tentative bytes; this
implementation only serves completed chunks.
