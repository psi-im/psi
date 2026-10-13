# Whiteboards

A whiteboard is a shared SVG drawing shown in the call window. Start one with the contact's call
action: it shows the whiteboard icon when calls are not possible (no media backend, or the
contact advertises whiteboards but no calls); otherwise use **Whiteboard only** in the menu of
the chat toolbar's call button, or the **Whiteboard** button of a running call. The contact sees
an invitation and joins after accepting it in the window. Both sides can draw at the same time.
**End** or closing the window leaves the session. **Save** writes the current drawing to an SVG
file, also after the session has ended; **Import SVG** adds the drawing of an SVG file.

The protocol is a Jingle session with the SVG whiteboard application type, synchronized with
Shared XML Editing (XEP-0284). When both clients can, the drawing goes peer-to-peer over an ICE
data channel, which is much faster than messages through the server for large drawings and
images; otherwise, or when the connection breaks, it goes through the server. It is implemented in Iris; see `iris/docs/jingle-sxe.md`. The
contact must advertise whiteboard support. Group chat whiteboards are not supported yet, and the
pre-2026 Psi whiteboard protocol is not supported anymore.

## Encryption

Calls, and whiteboards on an ICE data channel, are encrypted with DTLS. With OMEMO set up, Psi
also verifies the DTLS fingerprints end to end, compatible with Conversations and Dino: the device
that accepts an invitation names its OMEMO device in `<proceed/>`, and both sides send their
fingerprints encrypted for the other's device (`iris/docs/jingle-e2e.md`). The call window shows
the result under the status: verified with OMEMO, encrypted but from an untrusted device,
encrypted but not verified, or, for a whiteboard that goes through the server, not end-to-end
encrypted.

Files sent to a contact whose chat uses OMEMO are encrypted end to end with JET-OMEMO when the
contact supports it (Conversations and Dino do), on any transport; the transfer shows
"end-to-end encrypted" while it runs, and a received file that fails authentication is deleted.

**Options → Security → OMEMO → Configuration → Calls, whiteboards and files** (`options.media.require-end-to-end`)
makes this a requirement for contacts whose chat uses OMEMO (the chat's encryption choice is
remembered per contact; the OMEMO defaults apply until one is made): a call or whiteboard with
such a contact is refused, with a message in the window, unless the contact's fingerprints are
verified by a trusted OMEMO device; a whiteboard then needs an ICE data channel. A contact whose
chat is switched to no encryption is not affected. Files to and from such contacts are then
refused unless they are encrypted with JET-OMEMO.
Sessions sent directly to a resource (without XEP-0353) cannot be verified, since the peer's
device is unknown.

## Controls

The current tool acts on the left mouse button. Selecting, moving, scaling, rotating, zooming
and panning work with any tool, with modifiers, other buttons and the keyboard. **Controls**
in the toolbar (F1) lists them with the platform's key names.

| Action | Mouse | Keyboard |
| --- | --- | --- |
| Tools | toolbar | V select, P pen, L line, R rectangle, O ellipse, D rhombus, T text, I image, E eraser |
| Straight line with the pen; square, circle | Shift + drag | |
| Stroke width, text size | | `[` `]` |
| Select and move | Ctrl + drag an item | |
| Select an area | Ctrl + drag on the empty canvas (Shift adds) | Ctrl+A, Esc |
| Add to / remove from the selection | Ctrl + Shift + click | |
| Scale the selection | Ctrl + Shift + drag it | `+` `−` |
| Rotate the selection | right-drag, Alt + wheel | `,` `.` 15°, `<` `>` 1° |
| Move the selection | | arrows (Shift: ×10) |
| Duplicate, delete | | Ctrl+D, Delete or Backspace |
| Group, ungroup | item menu | Ctrl+G, Ctrl+Shift+G |
| Stacking order | item menu | Page Up / Page Down, Home / End |
| Item menu | right-click | |
| Zoom | Ctrl + wheel, pinch | Ctrl+= / Ctrl+− |
| Fit the canvas | double-tap with two fingers (macOS) | Ctrl+0 |
| Pan a zoomed canvas | middle-drag, Space + drag, wheel | |
| Insert an image | drop files on the canvas, click with the image tool | Ctrl+V |

On macOS Command replaces Ctrl, Control-click opens the item menu, and Option replaces Alt.
Trackpads pan with two-finger scrolling or Space + drag and zoom with a pinch; rotation is
also available from the keyboard because a right-drag is awkward on a trackpad.

Text is inserted as outlines, as the whiteboard profile recommends: it looks the same for every
participant, but can only be moved and transformed afterwards, not edited as text. Images are
embedded in the document; large images are scaled down to 1600 pixels.

## Code structure

| Class | Role |
| --- | --- |
| `CallDlg` (`src/avcall`) | Call window; starts and accepts whiteboard sessions and adds whiteboards to calls |
| `WbManager` | Keeps incoming whiteboard sessions until their event (`SxeEvent`) is opened; checks contact support |
| `WbView` | Whiteboard tile: canvas, toolbar, controls help, save and import |
| `WbWidget` | `QGraphicsView`: tools, gestures, zoom and keyboard handling |
| `WbScene` | Selection operations: grouping, stacking order, writing item transformations |
| `WbItem` | One top-level SVG element rendered with `QSvgRenderer` |
| `WbNewPath`, `WbNewShape` | Items being drawn; serialized to SVG when finished |

Edits go through `Jingle::SXE::Document` (`insertNodeAfter()`, `setAttribute()`, `removeNode()`,
then `flush()`); remote edits arrive as document signals (`nodeAdded`, `nodeMoved`,
`nodeToBeRemoved`, `chdataChanged`).

## Invitations that ring

Whiteboard invitations to a contact use Jingle Message Initiation
(XEP-0353), like calls: the proposal goes to the bare JID, all of the contact's online devices
show the invitation and answer `<ringing/>`, the one that accepts with `<proceed/>` gets the
session (its sid is the proposal id), and the others drop the invitation when the carbon of the
answer arrives. Closing the window before an answer withdraws the proposal (`<retract/>`);
declining sends `<reject/>`; `<finish/>` follows the end of the session. Without a device that
advertises XEP-0353 and whiteboards, the session goes directly to the capable resource with the
highest priority. `WbManager` implements this for whiteboard-only proposals
(`src/whiteboarding/unittest/proposal.cpp` tests it with two clients); a proposal of a call with
a whiteboard is not supported yet.

Calls and whiteboard invitations ring and can be answered from the notification:

- **Ringing.** An incoming call or whiteboard invitation repeats the ring tone while its event
  waits in the roster and, once opened, in the call window until it is accepted, declined,
  withdrawn by the caller, answered on another device or a minute has passed. The caller hears a
  ringback tone after the contact's devices answer `<ringing/>`, or right away for a session
  sent directly to one resource, until the contact answers (at most 90 seconds). `CallRinger`
  (`src/avcall/callringer.h`) repeats the sound every 3.5 seconds through
  `PsiAccount::playSound()`, so Do Not Disturb, "silent while away" and the sound switch apply.
  The sounds are the options `options.ui.notifications.sounds.incoming-call` (`sound/ring.wav`)
  and `options.ui.notifications.sounds.outgoing-call` (`sound/ringback.wav`), on the Sounds page
  of the options. Both default sounds are generated tones: two bursts of an E5–G♯5 major third
  with a light tremolo, and a soft 425 Hz tone.
- **Notification with buttons.** The notification of an invitation has **Accept** and
  **Decline**: the freedesktop notifier sends them as notification actions (category
  `call.incoming`, critical urgency, no expiry), the built-in popup shows buttons. Accepting
  opens the call window and accepts; declining answers without opening a window; clicking the
  notification opens the window as before. The notification closes when the invitation leaves
  the event queue, e.g. when the caller withdraws or another device answers. Notifiers without
  actions (Growl) keep their usual behavior. `PsiCon::answerCallEvent()` serves all of them.

## One window for calls and whiteboards

A call and a whiteboard are both Jingle sessions, and one session can carry several contents
(audio, camera, screen, whiteboard) added and removed with `content-add`/`content-remove`.
Psi therefore shows them in one window: a call can get a whiteboard, and later a whiteboard can
get a microphone or camera. Implemented so far: the stage with tiles, whiteboard-only sessions in
the call window, adding a whiteboard to a call and a microphone or camera to a whiteboard
(either side), switching contents on and off through senders, and the single call/whiteboard
action. A whiteboard switched on before a call is added with `content-add` once the call
connects. Still planned: a call with a whiteboard in one proposal (see the order of work below).

### Window

```text
┌──────────────────────────────────────────────┬──────────────┐
│                                              │ remote camera│
│   main stage: the selected tile              ├──────────────┤
│   (shared screen, whiteboard or camera)      │ whiteboard   │
│                                              │  (preview)   │
├──────────────────────────────────────────────┴──────────────┤
│  mic   camera   share screen   whiteboard   …        end    │
└─────────────────────────────────────────────────────────────┘
```

- Every visual content is a tile: remote camera, remote screen, the whiteboard. The main stage
  shows one tile, the side panel shows only the other tiles as small previews; clicking a
  preview moves it to the main stage. Nothing is shown twice.
- Default for the main stage, decided by the viewer: an incoming shared screen, then the
  whiteboard, then the remote camera. A new tile takes the main stage only until the viewer picks
  a tile manually; after that the choice stays until that tile disappears. The order is an
  option.
- The whiteboard tile shows its toolbar only on the main stage. While another tile is on the
  stage, the whiteboard is a read-only, fitted preview in the side panel.
- Audio-only calls show the whiteboard or a placeholder on the main stage.

### Turning contents on and off

Switching something off in a call does not remove its content, and only **End** ends the
session. The first time a kind of content is switched on, it is negotiated with `content-add`;
afterwards the buttons change the content's senders (XEP-0166 `content-modify`), and switching
on again resumes the same content.

| Button | First use | Off | On again |
| --- | --- | --- | --- |
| Microphone | part of the offer of a call, otherwise `content-add` | stop sending; still hear the peer | send again |
| Camera | `content-add` | stop sending | send again |
| Screen sharing | source picker, `content-add` | stop sending, keep the content | source picker again, same content |
| Whiteboard | `content-add`, or part of the initial offer | senders `none`: the board is hidden and frozen for both | senders back: the board reappears where it was |

- A session starts with exactly what was chosen: a whiteboard invitation offers only the
  whiteboard, a call offers audio (and video). Audio is added later with `content-add` like the
  camera.
- Muting the microphone never stops listening, and switching everything off never ends the
  session: the window stays, and anything can be switched on again.
- A whiteboard off means nobody edits it and its tile disappears for both participants; the
  document is kept, so switching it on continues from the same drawing. A whiteboard-only
  session shows the same control bar, so turning on the microphone makes it a call.
- Screen sharing on again opens the source picker like the first time. The new source is sent
  on the existing content without renegotiation: the media backend switches the video input of
  a running content (`setContentCapture()` in `psimediajingle.cpp`). The peer detaches its
  video output while the screen is not sent, so the tile reappears with the first new frame.
- Media buttons depend on both sides. Without a local media backend (Psi built or running without
  PsiMedia) the microphone, camera and screen buttons are hidden and the peer is not offered
  RTP: Iris then advertises no RTP features and refuses incoming RTP contents. The buttons are
  also disabled when the peer does not advertise RTP. Whiteboard-only sessions need neither.
- An RTP content added by the peer to a running session is accepted for receiving: the session
  itself was already accepted. Local capture stays off until the user switches the microphone
  or camera on, as for incoming calls today. In a call this applies to video (audio is part of
  the call); in a session that started as a whiteboard also to audio.
- In a whiteboard session the microphone and camera become available once it is connected, if
  this side has a media backend and the peer advertises calls.
- Possible later: a whiteboard where only one side draws (senders = initiator or responder),
  for presentations.

### Starting a session

One action starts calls and whiteboards. It replaces **Voice Call** and the separate
**Open a Whiteboard** / **Open a Whiteboard To** entries of the contact menu.

- With a usable media backend the action shows the phone icon, as now. It opens the call
  window in its pre-call state with the microphone on by default; camera and whiteboard can be
  switched on before calling.
- Without a media backend (or when the contact advertises no RTP) the same action shows the
  whiteboard icon and opens the same window with the whiteboard selected and the media controls
  disabled.
- The action is shown when the contact supports at least one of the two; the whiteboard
  requires the contact's `urn:xmpp:jingle:apps:svg-whiteboard:0` and SXE features.
- In the chat toolbar the call button gets a menu (split button) with **Whiteboard only**, for
  a whiteboard without audio when a call would be possible. The group chat participant menu
  uses the same action.
- Calls and whiteboards to a contact ring all of its devices, from the roster and from a chat
  alike: a XEP-0353 proposal goes to the bare JID and the first device that answers
  `<proceed/>` gets the session. The resource shown in the chat header is not used: Psi often
  locks it to the last resource that wrote, which is not a choice of the user. A direct
  `session-initiate` is an IQ to one resource and is not copied by message carbons, so it would
  ring only that device.
- The session goes directly to a full JID (`session-initiate`, no proposal) only for a group
  chat occupant, for a resource chosen in **Advanced → Call To** of the contact menu, and when no
  capable device of the contact advertises XEP-0353; then the capable resource with the highest
  priority is used. A contact without known devices still gets a proposal.

### Structure

- `CallDlg` drives whiteboard contents itself: a whiteboard-only session has no `AvCall`, and in
  a call it uses `AvCall::jingleSession()` to add the whiteboard and to accept one added by the
  peer. For a session that started as a whiteboard, `AvCallManager::joinSession()` creates an
  `AvCall` that adds audio or video with `content-add` and accepts RTP contents added by the
  peer; it never terminates the session, which stays owned by the dialog.
- `CallView` is a stage with tiles and a control bar. `CallStage` (`src/avcall/callstage.h`)
  places the visible tiles: the main tile by the default order or the viewer's choice, the others
  as clickable previews, and a placeholder when nothing is visible. The remote camera and the
  remote screen are `PsiMedia::VideoWidget` tiles; the whiteboard tile is a `WbView`, whose
  toolbar is hidden while it is a preview.
- `AvCall` grows into a session object with one controller per content type: the existing RTP
  logic (audio, camera, screen) and a whiteboard controller (`Jingle::SXE::Application` and its
  document). Incoming sessions with any mix of these contents open one window.
- The standalone whiteboard window is gone; whiteboards open in the call window.
- One XEP-0353 proposal can describe several applications, so "call with whiteboard" rings
  once. The ringing and the notification with Accept/Decline described above serve every kind of
  session.

### Order of work

1. Iris: senders for SXE contents (`content-modify`), regression tests for RTP and SXE contents
   in one session (content-add in both directions) and SXE descriptions in XEP-0353 proposals.
2. Stage and tiles in `CallView`, first with the existing camera and screen tiles (done;
   `avcallstage_test` covers the tile order and the viewer's choice).
3. The whiteboard as a tile and controller; whiteboard sessions open in the call window, and
   one call/whiteboard action replaces the separate whiteboard menu entries (done; the group
   chat participant menu still has no whiteboard).
4. Turning contents on and off from the control bar through senders; screen sharing keeps
   its content and reopens the source picker (done, together with media in whiteboard
   sessions).
5. XEP-0353 proposals for whiteboards (done) and mixed sessions (to do: a call with a
   whiteboard in one proposal, and incoming sessions with both RTP and SXE contents).
6. Ringing and actionable notifications (done).
7. SXE payload on Jingle connections (done: an ICE SCTP data channel when both sides support
   it, falling back to messages, sharing the call's BUNDLE association when a whiteboard is
   added to a call; see `iris/docs/jingle-sxe.md`).

