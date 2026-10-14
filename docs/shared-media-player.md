# External shared-media playback

In the WebEngine chat view, shared audio and video have **Open in external player**
and **Choose external player** buttons. The first launch asks for a player; Psi
detects mpv, VLC and SMPlayer on PATH and also allows choosing an executable.
The selection is stored in `options.ui.chat.external-media-player`. The second
button changes it. The executable must accept a file or HTTP URL as its argument.

Psi passes cached media as a local file and uncached media through its existing
local HTTP file-sharing proxy. Keep Psi running while playing a remote file.
The player launches only when a button is clicked. Successful embedded playback
continues to work. Errors from the embedded player show a message beside the
buttons; network and decoding errors have separate messages. A source-element
error without a more specific media error uses a generic failure message.

Qite and Qt WebKit do not expose the new bridge operation and retain their UI.
HTTP range fixes apply to the shared proxy: suffix ranges are resolved when the
size is known, endpoints are clamped, cached responses stop at the requested end,
and HTTP-source Content-Range metadata is parsed correctly. Unsupported multiple
ranges remain unsupported. See [RFC 9110, section 14](https://www.rfc-editor.org/rfc/rfc9110.html#name-range-requests).

## Validation

The standalone regressions work with either Qt major version:

```sh
cmake -S tests/filesharing -B build/shared-media-tests-qt6 -DQT_DEFAULT_MAJOR_VERSION=6
cmake --build build/shared-media-tests-qt6 -j2
ctest --test-dir build/shared-media-tests-qt6 --output-on-failure
```

Repeat with a separate Qt5 tree and `-DQT_DEFAULT_MAJOR_VERSION=5`.
The executable-launch fixture uses a POSIX shell and skips on other platforms.
The JavaScript test requires Node and uses a DOM double, not a WebEngine renderer.

For manual validation, rebuild and restart the **receiving WebEngine Psi**.
The JavaScript helper is embedded in `themes/chatview.qrc`. Check:

- WebM plays in the chat; both external-player buttons remain available.
- AVI failure displays a message; opening externally streams the file without
  waiting for a full download. Seeking near the end issues a bounded range request.
- Shared audio plays both internally and externally; opening externally pauses
  internal playback. Changing the player affects subsequent launches.
- Cancelling player selection launches no process.
- Cached head and tail range requests return exactly the advertised bytes,
  and requests past the end return HTTP 416.

This change requires the updated receiver. The sender can keep the existing
file-transfer implementation.
