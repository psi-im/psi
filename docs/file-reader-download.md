# File readers and download goals

Shared files with a known size and immutable identity have one `FileDownload`
controller per account and resource, and one `FileReader` per consumer. They
reuse the existing scheduler, source selection, transport validation and
`FileCache` range backend. `MediaChunkReader` has been removed.

`FileReader` owns a cursor and at most 64 KiB of buffered remote data. Complete
files are read directly through a local file handle. Incomplete files use the
shared controller. Opening or seeking never fetches; a read that reaches a gap
requests only that span, with bounded alignment to a transport chunk. There is
no autonomous lookahead. The controller starts with 64 KiB chunks and adapts
between 16 KiB and 1 MiB using observed throughput. Its approximate latency
budgets are 150 ms for interactive reads and 400 ms for background chunks,
excluding queued admission.

A gap returns zero on the owner thread and later emits `readyRead`. A decoder on
a foreign thread waits on a condition variable until its requested read is filled
or reaches EOF. It may reuse already resident bytes for buffered codec probing;
that read-ahead never creates additional network demand. The device has one cursor caller
at a time: the owner must not operate that cursor concurrently with the decoder.
`abort()` is safe from either thread and wakes pending reads. Consumers must
stop before closing or destroying the device. qite interrupts the device before
tearing down its decoder, then closes it afterward. Known-size playback uses the
device directly with FFmpeg (Qt's desktop default since 6.5, or an explicit
`QT_MEDIA_BACKEND=ffmpeg`). Qt 5 and GStreamer retain the loopback HTTP adapter
over the same reader: their seekable appsrc path reads on the GUI thread and
interprets a temporary zero read as EOF. Without a loopback server those
backends require a complete download before playback. Completed local files can
use a device with every backend, because their reads have no gaps.

`FileDownload::downloadAll` adds a separately cancellable full-download goal. It
uses stored range metadata, without reading cached payloads or depending on
playback consumption. It fills holes from the current playhead to EOF, then
wraps to the beginning. Seeks update that playhead even when they hit the
reader's buffer. A goal may keep at most three background chunk requests
pending. The shared scheduler reserves interactive admission and stops admitting
background work while interactive requests need the network. Already active
transfers drain in bounded chunks. Overlapping demands join the admitted
geometry; cancellation removes one waiter, preserving other consumers.

A complete range map does not settle a full goal while transport work remains
pending. In particular, a shared sequential fallback exposes its prefix to
readers as data becomes available, but full goals wait for accepted EOF and a
storage checkpoint. Large checksum spans run in a single-worker verification
queue, keeping the I/O worker available for playback. A full goal waits for
every announced checksum and the durable verification checkpoint. Writes
invalidate in-flight verification by payload revision; shutdown cooperatively
cancels a long pass. A source ignoring Range may use one shared sequential
fallback only while an explicit full goal exists. Cancelling the last goal
revokes that permission, including during pending range negotiation. Playback
alone reports `RangeUnsupported` instead of silently draining the entire file.
This is application policy; HTTP and Jingle response parsing stay in their
transport adapters. [RFC 9110 section
14.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-14.2) allows a server
to ignore Range; [XEP-0234 section
6.4](https://xmpp.org/extensions/xep-0234.html#range) defines Jingle ranged
negotiation.

Save As adds a full goal and then exports the completed local file through the
existing bounded `QSaveFile` copy job. An existing destination survives failure
or cancellation. Downloading directly to the chosen destination remains future
work. This implementation downloads to the cache before exporting the completed
file. Address-only, unknown-size and authenticated encrypted sources use the
common cancellable preparation path before export. AES GCM plaintext is exposed
only after whole-file authentication. Complete local files satisfy a full goal
without fetching or copying their bytes into range storage. See the
[chat media opening contract](chat-media-api.md#opening-content).

## Disk pressure and partial-file lifetime

Range writes never preallocate the full logical file. On Windows a write past
EOF requires successful `FSCTL_SET_SPARSE`; on POSIX the backend probes its
payload directory once using a small temporary file and allocated-block
accounting. A filesystem that cannot preserve holes is not allowed to allocate a
huge gap. The worker also leaves a 16 MiB free-space reserve for profile
metadata and other application writes, rather than filling a volume to zero.

On a storage-write failure, bounded transport data can instead enter the same
`FileCacheMemory` LRU. Hashing applicable complete checksum spans still runs on
the worker. Readers may consume those volatile bytes; they never enter SQL range
coverage, advertise a complete file or satisfy a full-download goal. RAM
eviction or restart drops them. A full-download goal reports
`StorageUnavailable`, leaving its destination untouched. Bytes on disk and bytes
in RAM can be returned as successive short reads; consumers do not need to
stitch cache tiers themselves.

`options.media.incomplete-max-age-days` defaults to 10. The worker expires
inactive partial payloads at startup, hourly and when the setting changes.
Successful reads and writes refresh a coarsened last-access timestamp in file
metadata. Published, complete and pinned files are skipped. Messages → Common
includes the setting and Delete now, which removes inactive partial files and
reports the payload blocks actually unlinked. Sparse logical sizes are not
counted as freed space. Ordinary quota pressure can evict unpinned cache data
earlier.

## Validation

For transfer diagnostics, set `QT_LOGGING_RULES="psi.media.*.debug=true"` before
starting Psi. Source logs identify the selected transport and response metadata;
range logs record the requested offset/length, first payload, elapsed time and
completion. Failures include received bytes and whether metadata and transport
success were observed. Source URLs omit credentials, query strings and fragments.
The logs do not dump file payloads. The bundled Iris option
`IRIS_DTLS_PACKET_DEBUG=ON` additionally traces incoming and outgoing DTLS datagram
sizes, allowing a stalled handshake to be distinguished from a stalled file
transfer. ICE connectivity checks alone do not prove that DTLS data is flowing.

The regressions cover demand-only tail reads, seek/peek and independent cursors,
stale completions, blocking decoder reads and abort, download order and paused
playback, independent waiter cancellation, foreground admission, early
sequential availability without premature download completion, RAM-only reads
with hash checks, and expiry with pins and allocated-byte accounting. Native Qt
6 tests decode and seek a WebM through delayed device data or the loopback
reader adapter, according to backend policy, and tear down a decoder during a
network stall. Ubuntu 24.04/Qt 6.4 exercises the GStreamer adapter; newer Qt
versions with FFmpeg exercise direct devices. Existing qite playback and
detached-window lifetime tests cover the opener's abort/close order. Large-hash
regressions hold verification while reading and rewriting cached bytes, reject
stale passes, cancel at shutdown, and ensure full goals wait for successful
verification and report late integrity or checkpoint errors.

The compatibility choice follows the public default-backend policy in [Qt 6.5
Multimedia](https://doc.qt.io/qt-6.5/qtmultimedia-index.html#ffmpeg-as-the-default-backend).
The limitation is observable in Qt 6.4's [`QGstAppSrc::pushData` and
`eosOrIdle`](https://github.com/qt/qtmultimedia/blob/v6.4.2/src/plugins/multimedia/gstreamer/common/qgstappsrc.cpp):
a random-access zero read sends EOS. qite's stream cleanup is a child resource
whose destructor runs after the player base destructor, because Qt 5
[`QMediaPlayer::~QMediaPlayer`](https://github.com/qt/qtmultimedia/blob/5.15/src/multimedia/playback/qmediaplayer.cpp)
disconnects the player's signals before QObject emits `destroyed`.
