# Whiteboards

A whiteboard is a shared SVG drawing. Open it from a contact's menu (**Open a Whiteboard**); the
contact sees an invitation and joins after accepting it. Both sides can draw at the same time.
Closing the window leaves the session. **Save** writes the current drawing to an SVG file, also
after the session has ended.

The protocol is a Jingle session with the SVG whiteboard application type, synchronized with
Shared XML Editing (XEP-0284). It is implemented in Iris; see `iris/docs/jingle-sxe.md`. The
contact must advertise whiteboard support. Group chat whiteboards are not supported yet, and the
pre-2026 Psi whiteboard protocol is not supported anymore.

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
| `WbManager` | Starts outgoing Jingle whiteboard sessions, presents incoming ones (`SxeEvent`) and owns the dialogs |
| `WbDlg` | Window, toolbar, controls help; takes over the shared `Jingle::SXE::Document` |
| `WbWidget` | `QGraphicsView`: tools, gestures, zoom and keyboard handling |
| `WbScene` | Selection operations: grouping, stacking order, writing item transformations |
| `WbItem` | One top-level SVG element rendered with `QSvgRenderer` |
| `WbNewPath`, `WbNewShape` | Items being drawn; serialized to SVG when finished |

Edits go through `Jingle::SXE::Document` (`insertNodeAfter()`, `setAttribute()`, `removeNode()`,
then `flush()`); remote edits arrive as document signals (`nodeAdded`, `nodeMoved`,
`nodeToBeRemoved`, `chdataChanged`).

## Planned: invitations that ring

Whiteboard invitations will use Jingle Message Initiation (XEP-0353), like calls: all of the
contact's online devices ring, the one that accepts joins the session, and the others stop
ringing.

- **Ringing.** While an incoming call or whiteboard proposal is pending, Psi repeats a ring tone
  until it is accepted, declined, withdrawn by the caller or times out. The caller hears a
  ringback tone after the contact's devices answer `<ringing/>`. A shared helper serves calls
  and whiteboards; it respects Do Not Disturb, "silent while away" and the sound switch. New
  sound options: `options.ui.notifications.sounds.incoming-call` (ring tone) and
  `options.ui.notifications.sounds.outgoing-call` (ringback), with generated default sounds.
- **Notification with buttons.** The invitation popup gets **Accept** and **Decline**. The
  freedesktop notifier provides them as notification actions; the built-in popup gets buttons.
  Where the platform notification has no actions, the event in the roster opens the usual
  question. The notification closes when another device answers or the caller withdraws.

## Planned: one window for calls and whiteboards

A call and a whiteboard are both Jingle sessions, and one session can carry several contents
(audio, camera, screen, whiteboard) added and removed with `content-add`/`content-remove`.
Psi will therefore show them in one window: a whiteboard can get a microphone or camera, and a
call can get a whiteboard.

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
on again resumes the same content. Microphone and camera already work this way.

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
  on the existing content; whether the backend can switch the capture source of a running
  content, or needs a renegotiation (`description-info`), is to be checked in the RTP code.
- Media buttons depend on both sides. Without a local media backend (Psi built or running without
  PsiMedia) the microphone, camera and screen buttons are hidden and the peer is not offered
  RTP: Iris then advertises no RTP features and refuses incoming RTP contents. The buttons are
  also disabled when the peer does not advertise RTP. Whiteboard-only sessions need neither.
- An RTP content added by the peer to a running session is accepted for receiving: the session
  itself was already accepted. Local capture stays off until the user switches the microphone
  or camera on, as for incoming calls today.
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
- The target follows the user's choice. In a chat with a resource selected in the header, the
  session goes directly to that full JID (`session-initiate`, no proposal). Otherwise, and from
  the roster menu, a XEP-0353 proposal goes to the bare JID and the first device that answers
  `<proceed/>` gets the session; without XEP-0353 the session goes to the capable resource with
  the highest priority. Calls follow the same rule; today they always send the proposal to the
  bare JID when XEP-0353 is enabled.

### Structure

- `CallView` is a stage with tiles and a control bar. `CallStage` (`src/avcall/callstage.h`)
  places the visible tiles: the main tile by the default order or the viewer's choice, the others
  as clickable previews, and a placeholder when nothing is visible. The remote camera and the
  remote screen are `PsiMedia::VideoWidget` tiles; the whiteboard tile will wrap `WbWidget`
  with the whiteboard toolbar taken out of `WbDlg`.
- `AvCall` grows into a session object with one controller per content type: the existing RTP
  logic (audio, camera, screen) and a whiteboard controller (`Jingle::SXE::Application` and its
  document). Incoming sessions with any mix of these contents open one window.
- The standalone whiteboard window goes away; whiteboards open in the call window.
- One XEP-0353 proposal can describe several applications, so "call with whiteboard" rings
  once. The ringing and the notification with Accept/Decline described above serve every kind of
  session.

### Order of work

1. Iris: senders for SXE contents (`content-modify`), regression tests for RTP and SXE contents
   in one session (content-add in both directions) and SXE descriptions in XEP-0353 proposals.
2. Stage and tiles in `CallView`, first with the existing camera and screen tiles (done;
   `avcallstage_test` covers the tile order and the viewer's choice).
3. The whiteboard as a tile and controller; whiteboard sessions open in the call window, and
   one call/whiteboard action replaces the separate whiteboard menu entries.
4. Turning contents on and off from the control bar through senders; screen sharing keeps
   its content and reopens the source picker.
5. XEP-0353 proposals for whiteboards and mixed sessions.
6. Ringing and actionable notifications.
7. SXE payload over ICE/S5B (`iris/docs/jingle-sxe.md`), which also lets a whiteboard share the
   call's BUNDLE association.

