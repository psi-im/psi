# Shared-media diagnostics

Branch: `ai/shared-media-diagnostics`. These changes add logging only; they do
not change HTTP ranges, Jingle transfer policy, or media playback.

Build the branch on the WebEngine receiver and preferably on the file owner.
`themes/chatview/util.js` is embedded through `themes/chatview.qrc`, so rebuild
and restart the receiving binary as well; updating installed theme files alone
does not replace the embedded helper. Reproduce opening the player in a new chat.
Save both debug logs from before opening the player until it fails, together
with the Jingle XML log. Keep the full logs; `[fs-media]` is the diagnostic prefix.

The browser logs MIME support, media/source errors, ready/network state, buffer
and seek ranges. The HTTP proxy logs Range, parsed offset/length, response status,
Content-Range/Content-Length, backpressure and connection/response teardown.
Downloader logs link proxy pointers to a Jingle session ID, accepted ranges,
source failures, reads and aborts. The owner logs the same session ID, file size,
requested offset/length, file position, errors and terminal reason.
Byte progress is sampled roughly once per MiB; browser progress at most once
per five seconds. Elapsed times and byte counts help distinguish slow transport
from a browser cancelling a request. The new diagnostics do not log file contents
or cryptographic key material.

## Compare HTTP ranges on the receiving machine

Copy the same player URL that worked with wget into `URL`. For the reported
2343036928-byte movie, these requests ask for the first and last 64 KiB:

```sh
URL='paste the player URL here'
curl -v --max-time 20 --max-filesize 65536 -H 'Range: bytes=0-65535' "$URL" -o first.bin
curl -v --max-time 20 --max-filesize 65536 -H 'Range: bytes=2342971392-' "$URL" -o tail.bin
curl -v --max-time 20 --max-filesize 65536 -H 'Range: bytes=-65536' "$URL" -o suffix.bin
wc -c first.bin tail.bin suffix.bin
```

A correct partial response has status 206, Content-Length 65536 and Content-Range
`bytes 0-65535/2343036928` or `bytes 2342971392-2343036927/2343036928`.
The size limit avoids downloading the whole movie if the server returns 200.

Both suffix and explicit-offset ranges are defined by
[RFC 9110 section 14.1.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-14.1.2).
The existing parser explicitly rejects suffix ranges (`bytes=-N`) and the proxy
maps that to HTTP 501. An explicit starting offset (`bytes=OFFSET-`) uses a
different path. The supplied XML contains an empty `<range/>`; it does not show
which HTTP Range the player requested or any later HTTP requests.
The new logs are needed to connect the actual browser sequence and media errors
to the transfer cancellation. A successful full GET with wget does not establish
that range requests or the browser's media decoder work.
