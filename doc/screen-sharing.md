# Calls and screen sharing

In a connected call, choose **Share screen**, select a display or an individual
window, and confirm **Share**. The camera and screen can be transmitted together.
The green sharing strip identifies the selected source. **Stop sharing** ends
only that video content; microphone and camera controls remain independent.
Cancelling the picker never starts capture. Incoming video does not grant access
to a local camera or screen. Incoming calls initially have the camera switched off.

On Linux/Wayland, selection and permission are provided by the desktop's
`org.freedesktop.portal.ScreenCast` portal, with PipeWire video capture. Install
`xdg-desktop-portal`, the appropriate desktop backend, and GStreamer's
`pipewiresrc` plugin. Available choices depend on the portal backend: some
compositors provide displays only. Permission is requested for each sharing
session and is revoked on stop, call termination, or desktop cancellation.

On Linux/X11, Psi displays separate **Displays** and **Windows** tabs with source
previews and captures the selected source with GStreamer's `ximagesrc`. XRandR
provides native monitor pixel coordinates; builds without its development headers
use `xrandr --listmonitors`, falling back to the entire desktop if unavailable.
An individual window remains selected by its X11 window ID, rather than by its
position or title. Closing a captured window ends its media content on a backend
error. Capturing an entire display also includes any windows placed on it.

The call view gives received screen video the main stage and keeps received camera
video beside it. Sharing displays a connecting state until the new content is
active. A peer declining the content or a capture error ends sharing without
terminating the existing call. The screen profile is VP8, up to 1920 × 1080 at
15 fps, within the call's selected bitrate limit.

This implementation provides local source selection on Linux. Windows and macOS
capture pickers are not implemented. Receiving screen video uses ordinary Jingle
RTP video and does not require a local capture picker.

## Build and interoperability

Update **both Psi and psimedia** and rebuild the media plugin. The optional
`GroupedSecureRtpSessionContext/1.0` interface enables independent video codec
contexts sharing protected RTP groups. An older secure media plugin can still
handle its existing call modes; it does not enable the Share screen button.
Linux portal support requires the existing D-Bus build option; X11 selection
requires the existing X11 build option. Qt 5 and Qt 6 remain supported.

Screen sharing uses a separate, locally sending RTP video content added to an
active Jingle session with `content-add` / `content-accept`. Iris extends an
already negotiated BUNDLE group when possible. A call without an established
BUNDLE can add a video content with its own transport. Capture starts only when
the new application is active and local consent and sender policy permit it.
Stopping revokes capture immediately, including while negotiation is pending;
Iris handles the subsequent content removal and rollback.

New audio-only calls can negotiate a BUNDLE group containing just the audio
content when both endpoints support grouping. Screen sharing can then extend
that group without another ICE/DTLS handshake. The peer must accept the group
in the initial answer; an audio call whose group was declined remains unbundled.
Existing calls established without BUNDLE also use a separate screen transport.
STUN remains part of the call's initial ICE negotiation even when BUNDLE is used;
adding a member to an established group reuses that ICE association.

The `screen-` and `camera-` content name prefixes are Psi presentation hints,
not an XEP-defined declaration of source type. Other clients must support active
video additions to receive a new screen stream. Their naming conventions can
produce different presentation layouts. External-client interoperability and
real compositor permission dialogs require manual call testing; synthetic
GStreamer and mock portal tests cannot establish those results.

## Diagnosing a failed addition

Collect both clients' application logs and the XMPP exchange for the same attempt.

The `jingle sid=...` entries identify outgoing actions and IQ results. A successful
`content-add` IQ result acknowledges receipt; look for `content-accept`,
`content-reject`, or an IQ error to learn the actual decision.

`jingle-rtp content=...` entries show media parameter application, secure route
binding, activation, and local/remote removal with its reason. BUNDLE commit and
rollback entries distinguish provisional membership from accepted membership.
Match the association and endpoint identifiers to psimedia's group configuration
messages. Those messages distinguish missing codec payloads, invalid MID/SSRC
routes, rejected codec mappings, and runtime errors. Worker/context addresses
identify which codec graph was cleaned up; cleanup alone does not identify the
failure that caused it. GStreamer errors include the element, error code and
debug detail. These diagnostics do not print ICE passwords or SRTP key material.

Psi automatically prepares a receive-only answer to a new video content in an
active call. This includes screen sharing during an audio-only call and additions
to an existing audio/video BUNDLE group. The IQ result acknowledging `content-add`
does not accept the stream; acceptance uses a separate `content-accept` with the
RTP answer and transport. Receiving a new stream never grants permission to start
local camera or desktop capture. Initial call acceptance remains a user decision.

## Memory-pressure diagnostics

On Linux, a memory-pressure kill may come from `systemd-oomd`, which kills an
entire application cgroup. A Qt Creator group can include Psi, language servers
and build processes. Its reported memory usage is their aggregate, not Psi's
individual RSS. Check `journalctl -b -u systemd-oomd` as well as the kernel log.
To investigate growth during a call, record Psi's RSS over time while repeating
camera/screen start and stop, separately from compilations and IDE indexing.
psimedia releases discarded UI frames and stop-queue commands, and bounds each
RTP `appsrc` input to 512 KiB. An `RTP input overload` message indicates that a
stalled consumer is losing packets; increasing the queue limit is not a remedy.

## Ownership

Each video content owns its codec worker and capture graph. The call's media
session owns protected RTP associations, replay state, group routing and RTCP;
adding a screen does not create a second association for an established BUNDLE.
Removing a screen disposes of that codec context while preserving surviving
members. Backend failures distinguish endpoint-local capture errors from failures
of the shared protected transport.

The desktop portal session closes immediately when sharing stops. A separate
lease keeps its PipeWire file descriptor alive until the old codec worker has
released the source. This prevents descriptor-number reuse while GStreamer is
still disposing of its cached PipeWire connection. Retaining the descriptor does
not retain portal permission.
