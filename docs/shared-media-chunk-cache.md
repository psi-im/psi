# Shared media chunk cache

The local HTTP file-sharing proxy uses a shared range cache for remote audio and
video with a known nonzero size and content hash when the URL has `mode=stream`.
WebEngine media sources and external-player links select this mode explicitly.
`mode=download`, an absent mode, an unknown mode, or multiple mode values select
the existing direct downloader. A sequential download is one transfer, rather
than a chain of 4 MiB content requests. Range headers still work in either mode;
they do not by themselves select playback. Qite's network access manager path
retains its existing behavior. Complete files in the existing file cache take
precedence in both modes.

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
  more as its consumer drains its 64 KiB buffer, with one chunk of lookahead
  inside the requested range. Canceling the reader also cancels its lookahead.
- Each missing chunk uses the existing downloader. Account-scoped Jingle queues
  serialize requests to the same full peer JID and reuse a negotiated BUNDLE via
  `content-add`. Peers without BUNDLE retain separate sessions. Adaptive bandwidth
  pacing is not implemented.
- The complete bounded transfer must succeed before its chunk is served. Jingle
  success includes existing negotiated checksum validation. Pooled Jingle payloads
  complete at `payloadVerified`; their receipt is retired separately. An initial
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

When built with Psi's Iris target, `filesharing_jingle_session` exchanges actual
ICE/DTLS/SCTP data between two local clients. Three queued ranges use one session
and two `content-add` exchanges; each predecessor receipt follows successor
`content-accept`. Without BUNDLE the same requests use three sessions. A canceled
queued request never starts. The test explicitly skips without a DTLS/SCTP backend.
After success or cancellation of the final prefetched range, both session objects
must terminate and be destroyed. Iris's backend-independent `jingle_sessionretirement`
also checks success and last-content removal in both sending roles over IBB,
retaining the first finished FT object while a second content remains active.
It also checks that handover keeps the same physical ICE association, and that
the queue can be destroyed inside its setup callback. The standalone component
tests run with ASan/UBSan/LeakSanitizer. The Jingle test uses ASan/UBSan with the
existing Iris enum suppressions for `stunmessage.cpp` and `stringprep.cpp`, and
with LeakSanitizer disabled for the previously documented Iris baseline leaks.
The Psi handover test also holds the final range open and processes deferred
deletions: both endpoints must already have destroyed the two preceding FT
applications, with only the live content remaining in the session.

## Jingle handover

`FileSharingJingleSession` owns the request queue independently of downloader
lifetimes. The downloader installs streaming callbacks before the pool submits
its application. `setKeepTransportUntilReceipt()` preserves the previous BUNDLE
member, and `setReceiptDeferred()` holds its receipt until the next application
becomes active. A queued turn lets answer processing commit before the previous
receipt is released. Another extension waits for the previous member's retirement.
With no queued successor, the receipt is released promptly; no idle session timer
or synthetic keepalive content is added. A stalled consumer can therefore require
a new session when it resumes. This follows the same handover mechanism as
[AnyKeep's IrisMediaSync](https://github.com/Ri0n/AnyKeep/blob/bc26ae6a7b133940b0198c4ab309a7cbe6899e68/plugins/xmpppubsub/iris/irismediasync.cpp).

The sender accepts additional requests only for locally available shared files.
It validates new applications separately and never reconfigures already accepted
ones. A rejected or canceled successor also retires its predecessor's receipt.
The HTTP cache and disk worker have no knowledge of Jingle sessions.
Both shared-file adapters schedule finished applications for deletion. This is
an ownership decision in Psi, not a change to generic Iris application lifetime:
other callers may still retain a finished application for UI or result inspection.

For a real playback check, rebuild both Psi endpoints, open the same AVI externally,
seek in both directions, and repeat the request to verify reuse. The sender uses
the existing ranged-transfer protocol, with added support for incoming `content-add`.
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
per-chunk content negotiation, and best-effort per-process disk quotas if multiple
processes share a cache directory. Initial streaming
before a chunk is verified would need a separate policy for tentative bytes; this
implementation only serves completed chunks.

## Durable partial downloads: proposed next storage change

The current rolling playback cache survives restart, but is disposable and has
no per-file manifest. It is not a durable resumable-download store. The ordinary
downloader still truncates its temporary file when starting a new download.
Splitting the URLs does not change that behavior.

A durable implementation should extend the storage layer and existing downloader,
with the following separation:

- File identity: format version, stable account ID, content-hash algorithm and
  digest, exact total size. Store the identity itself as well as the opaque disk
  key. A filename, temporary URL or full peer resource JID is not a file identity.
- Availability: ordered downloaded extents, independent of network request sizes.
  An ordinary download can fetch a missing contiguous tail in one transfer while
  committing multiple local checkpoints. Playback can continue using bounded
  range requests against the same file identity.
- Integrity: immutable leaf records with offset, length and a local SHA-256.
  Coalesce adjacent extents in the availability map without merging leaf payloads
  or replacing their hashes. Updating two neighbouring interval boundaries does
  not require reading the downloaded bytes. An indexed journal/database avoids
  rewriting an ever-growing JSON array after every checkpoint.
- Commit/recovery: commit the payload before publishing its extent. A truncated
  or corrupt leaf is a missing range after recovery, never an available range.
  Keep unfinished explicit downloads until completion or explicit removal;
  playback-only records retain an eviction budget. In-flight transfers are not
  persistent progress. Concurrent readers and writers must share ownership of
  the same entry rather than truncate one another's files.
- Completion: verify the advertised whole-file hash in byte order before
  promoting the result into the existing complete-file cache. Per-leaf hashes
  detect local damage but are not a replacement for that verification. Standard
  SHA digests of adjacent pieces cannot be combined into the standard digest of
  their concatenation. Use a streaming hash as a contiguous prefix advances,
  or one final ordered pass on the disk worker.

JET policy must remain separate from at-rest storage. Iris currently authenticates
JET as one whole-file stream and rejects ranges/streaming hooks for that mode.
Plaintext local checkpoints do not make arbitrary JET range transfers possible.
Adding JET resume or independently authenticated live ranges needs explicit
protocol support; the URL mode must not silently bypass this restriction.
