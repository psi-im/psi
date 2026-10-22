# Shared file storage and media ranges

Remote media with a known size and content hash uses the existing shared range
cache for playback and explicit saves. Network requests remain bounded, but their
payloads now go directly into one result file. All readers, seeks and saves for
the same account and immutable file identity share its downloaded ranges.
Unknown-size transfers retain the ordinary downloader.

`FileSharingChunkSource` uses Psi's existing downloader, HTTP support and Jingle
session pool. There is no additional signaling or transport stack.

## Payload and sidecar

Files live in `shares/media-files-v2` under Psi's data directory:

- `<key>.data` is the payload at its final byte offsets, without chunk headers.
  Holes are unavailable data, not downloaded zero bytes. NTFS files are marked
  sparse when supported; other filesystems retain their normal behavior.
- `<key>.json` is the only sidecar. `QSaveFile` atomically replaces it after a
  checkpoint. It contains the exact size, sorted downloaded ranges, received
  checksums, supplementary signaled metadata and any integrity error.
- The key hashes the account-scoped content identity and advertised total size.
  It does not depend on network request sizes, chunk boundaries or source URLs.
  A changed fetch granularity can therefore reuse the same partial file.

The version 2 JSON layout is:

```json
{
  "version": 2,
  "size": "32",
  "ranges": [["0", "8"], ["16", "16"]],
  "checksums": [
    {"offset": "8", "length": "16", "algorithm": "sha-256", "digest": "BASE64"}
  ],
  "metadata": {"name": "sample.bin", "media-type": "application/octet-stream"},
  "error": ""
}
```

Ranges are `[offset, length]`. Integers are decimal strings to preserve all 64
bits in JSON. Adjacent and overlapping ranges merge. Checksums retain their own
offsets and lengths independently of those merges. Metadata also preserves the
signaled file XML, sources and available thumbnail/waveform information.

Before overwriting available bytes, the worker checkpoints their invalidation.
It then writes and flushes the payload, merges availability, verifies any newly
covered checksum spans and checkpoints the result. A transfer is not reported
as stored when a disk write or sidecar commit fails. Startup reloads the sidecar
and rechecks fully covered hash spans. Unsupported sidecars and truncated
payloads are not advertised as available. The old `media-chunks-v1` records are
not migrated or deleted by this change.

## Verification and user retry

Whole-file hashes from SIMS metadata and range hashes from Jingle offers and
late checksum messages enter the same checksum list. Each hash is verified when
all bytes in its span have arrived. Disk I/O and hashing run on the existing
`shared-media-cache` worker thread; callbacks and UI updates return to the
application thread. The application supplies Iris's hash implementation, so
verification uses its supported XMPP algorithm names.

A mismatch removes the entire checksum span from availability, persists an
error and stops readers and saves for that file. Bytes outside the failed span
remain available for a later retry. There is no automatic retry or switch to
another source after an integrity failure. Explicit playback retry or a new save
request clears the halt and fetches only the missing ranges. Conflicting hashes
for the same span and algorithm are retained and reported as an error; retry
cannot make contradictory checksums agree.

Downloaded bytes can be consumed before a larger, still incomplete hash span is
verifiable. Completion and a successful explicit save require the entire file
and all received applicable checksums to pass. An integrity error also stops an
already open native player and prevents serving the failed payload as a source.

## Scheduling, sources and retention

- Fetches align to 4 MiB boundaries. Each fetch subtracts the already downloaded
  extents and requests only its holes. Seek requests take priority over ordinary
  continuation. Readers keep a 64 KiB consumer buffer and at most three chunks of
  lookahead within their requested range.
- At most four shared fetches run simultaneously, also limited to four per account.
  A newly encountered source is probed; later selection estimates completion
  time from measured throughput and its active request count. Available sources
  may therefore supply different ranges concurrently. Existing HTTP/Jingle
  fallback remains responsible for trying another usable URL after an ordinary
  source failure. This is source selection, not bandwidth reservation.
- The complete bounded transfer must succeed before its payload is committed.
  Jingle success includes its existing checksum validation. A server ignoring
  Range switches to one shared sequential transfer; that transfer writes bounded
  blocks into the same payload and sidecar. Concurrent requests join it. A wrong
  partial range is an error, rather than an unrestricted-download fallback.
  This fallback waits for full transfer validation before satisfying its waiters.
- Canceling a reader detaches its waiter; a shared fetch survives other consumers.
  The scheduler retains admission until an orphaned transfer acknowledges
  cancellation. At most 256 read callbacks are retained.
- Active readers and fetches pin their files against eviction. Explicit saves
  set `retain-partial` metadata, preserving unfinished progress after cancellation
  and restart. Integrity failures are also retained until the user retries.
  Disposable files use whole-file LRU eviction, with a soft 256 MiB payload
  budget and 2048-entry limit. Published sources are retained too. Pinned files,
  retained partial downloads, failed files and a sole oversized file can exceed
  those limits. RAM holds at most
  16 MiB of cached payload in addition to active transfers and reader buffers.

Once complete, Qite initializes native playback directly from the payload file.
Replay resolves the original shared-file identity again, so completion during
the first HTTP playback is picked up on replay. Browser-based WebEngine media
continues to use HTTP; the proxy serves the same completed file from disk.

A completed receiver can serve incoming file requests and use the existing
explicit publication path to advertise its own source. SIMS supports several
source references for the same file. This change does not send unsolicited
announcements or invent a protocol for advertising partially available pieces.

## Protocol and session ownership

HTTP Range handling follows [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html#section-14).
Jingle ranges and late checksums follow
[XEP-0234](https://xmpp.org/extensions/xep-0234.html); hash identifiers follow
[XEP-0300](https://xmpp.org/extensions/xep-0300.html). Shared-file references and
multiple sources follow [XEP-0385](https://xmpp.org/extensions/xep-0385.html).

`FileSharingJingleSession` owns the request queue independently of downloader
lifetimes. Requests to the same full peer JID reuse a negotiated BUNDLE via
`content-add`. `setKeepTransportUntilReceipt()` preserves the previous member;
`setReceiptDeferred()` holds its receipt until the successor becomes active.
With no successor the receipt is released promptly. Peers without BUNDLE retain
separate sessions. The storage worker has no knowledge of Jingle sessions.

JET remains separate from storage policy. Iris authenticates JET as a whole-file
stream and rejects ranges/streaming hooks in that mode. Plaintext local
checkpoints do not enable arbitrary authenticated JET ranges or resume.

The scheduler originates from
[AnyKeep PR #156](https://github.com/Ri0n/AnyKeep/pull/156), reviewed at
`db77d42ef791225839d03ecb0c62cd06462b31e3`; Psi keeps its own storage format and
existing transports.

## Validation

`tests/filesharing` covers merged out-of-order ranges, one payload plus one
sidecar, restart, late checksums, conflicting announcements, corruption,
invalidation of a complete hash span, explicit repair of only missing holes,
worker-thread hashing, source selection, active-file retention, quotas,
cancellation, admission, saves and bounded-transfer validation.

With bundled Iris, `filesharing_jingle_session` exchanges actual ICE/DTLS/SCTP
data between two loopback clients. Three queued ranges use one session and two
`content-add` exchanges; without BUNDLE they use three sessions. A canceled
queued request does not start. Finished predecessors retire while their
successor remains live, and both endpoints eventually destroy their sessions.
The test skips when the required backend is unavailable.

Qite playback tests cover a delayed HTTP response, switching replay to a
completed local file, and stopping on source integrity failure until an explicit
retry. Live multi-peer throughput and WebEngine playback still need an end-to-end
check with actual peers. Enable `psi.filesharing.chunks.debug=true` in Qt logging
rules to inspect fetch offsets, lengths and errors.
