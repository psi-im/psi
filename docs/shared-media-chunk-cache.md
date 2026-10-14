# Shared media chunk cache

The local HTTP file-sharing proxy uses a shared range cache for remote audio and
video with a known nonzero size and content hash. Both WebEngine and external
players use this HTTP path. Qite's network access manager path and ordinary file
downloads retain their existing behavior. Complete files in the existing file
cache take precedence.

`MediaChunkScheduler` is adapted from
[AnyKeep PR #156](https://github.com/Ri0n/AnyKeep/pull/156), reviewed at commit
`db77d42ef791225839d03ecb0c62cd06462b31e3`. It provides shared in-flight requests,
independent reader cancellation, priorities, and bounded admission. Psi does not
use AnyKeep's encrypted record format. `FileSharingChunkSource` delegates to
Psi's existing downloader and transports; it does not introduce a second Jingle
stack.

## Storage and loading

- Chunks are aligned to 4 MiB boundaries. At most two cache fetches run at once,
  also limited to two per account. This limits concurrency, not bandwidth.
- A new seek has priority over continued sequential reading. A reader requests
  more only as its consumer drains its 64 KiB buffer.
- Each missing chunk currently needs its own existing downloader/Jingle transfer.
  Transport reuse and adaptive bandwidth pacing are not implemented.
- The complete bounded transfer must succeed before its chunk is served. Jingle
  success includes the existing checksum/receipt processing. This means an initial
  read waits for a whole chunk, rather than streaming its first bytes immediately.
- Identity includes the account, content hash, advertised size, chunk size and
  index. Received size and range must agree with the request. Servers that do not
  support the required bounded ranges fall back to the existing direct path.
- Files live in `shares/media-chunks-v1` under Psi's cache directory. Records are
  plaintext, atomically committed, and disposable. A record checksum detects local
  corruption; it does not authenticate remote content.
- The disk budget is 256 MiB and at most 2048 records, with oldest-use eviction.
  Across restarts, eviction age is approximated by write time. Memory holds at most
  16 MiB of cached payload, in addition to active transfers and reader buffers.
  An unavailable/full disk leaves memory caching usable. Invalid or incomplete
  records are cache misses.
- Canceling a reader detaches its waiter. A shared fetch survives while another
  reader needs it. The scheduler retains admission until an orphaned transfer has
  acknowledged cancellation.

HTTP Range behavior follows RFC 9110, including ignoring Range on HEAD. Jingle
ranges continue to use the existing XEP-0234 implementation.

## Validation

Standalone tests in `tests/filesharing` build with Qt 5 or Qt 6. The scheduler and
cache tests cover overlapping reads, cancellation, admission, persistence,
corruption, quotas, asynchronous completion and shutdown. Replaying the 67 AVI
seek offsets from the reported mpv session uses three physical chunk fetches in
the test fixture; this is not a measurement of live Jingle sessions.

For a real playback check, rebuild the receiving Psi, open the same AVI externally,
seek in both directions, and repeat the request to verify reuse. The sender uses
the existing ranged-transfer protocol and does not require this cache change.
Enable `psi.filesharing.chunks.debug=true` in Qt logging rules to record cache
fetch offsets, lengths and completion errors. Verify that repeated nearby HTTP
ranges share chunks and no longer create a Jingle session per index entry.
