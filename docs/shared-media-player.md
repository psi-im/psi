# External playback in WebEngine chats

Shared audio and video show two compact buttons in the player corner:
**Open in external player** and **Choose external player**. On embedded playback
failure, the buttons expand into labelled actions centered over the player,
together with an error message. Network and decoding failures have different
messages. Opening externally pauses embedded playback and never happens
automatically.

The first launch offers mpv, VLC or SMPlayer found on PATH, or an executable
chosen from disk. The selection is remembered in
`options.ui.chat.external-media-player`; cancelling replacement keeps the old
selection. The executable must accept a file path or HTTP URL as one argument.
Cached media opens from disk. Other media streams through Psi's existing local
HTTP proxy, so Psi must stay running during external playback.

Qite and Qt WebKit retain their existing controls. Rebuild and restart the
receiving **WebEngine** client to load the updated embedded JavaScript and CSS.

Validation: `tests/filesharing` builds standalone with Qt5 or Qt6. It covers HTTP
ranges, process arguments, player-selection cancellation, bridge gating and
media error/recovery state. The JavaScript test uses Node and a DOM double.
Local Chrome checks additionally cover normal, narrow and audio overlay layout;
they do not replace an end-to-end Qt WebEngine playback test.
