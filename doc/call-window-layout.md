# Call window layout and theme contract

The main call dialog UI is `src/avcall/callview.cpp`, hosted by `calldlg.cpp`.
The same view handles outgoing calls, incoming calls, active sessions, media
joining an already-running whiteboard, and whiteboard-only sessions.

## Layout

- The native window title contains **Call — peer** or **Whiteboard — peer**.
  The JID editor appears inside the window only while the user can still
  change the outgoing destination. Incoming, connecting and active sessions
  must not duplicate the identity as a large in-window header.
- The compact status row contains the current connection status (a green dot
  and accessible tooltip when connected), the security icon (padlock for
  verified end-to-end encryption, warning symbol otherwise) and elapsed time.
  Keep the complete security explanation in the tooltip/accessibility name;
  notably, OMEMO whiteboarding can still use a server-backed transport.
  Such mixed-protection sessions use a warning icon, not a verified padlock.
  Errors and connection-progress messages stay visible as text.
- The stage, including camera, screen presentation and the embedded SVG
  whiteboard, has priority for resize space.
- Only one bottom control dock is used. Microphone, camera, screen-sharing and
  whiteboard toggles are icon-only with descriptive tooltips and accessible
  names. Their checked states remain synchronized with the actual session.
  The Hang up button stays visibly destructive and labelled in text.
- Sharing source/status is an elided dock caption with the complete source in
  tooltip/accessibility text. The share button itself stops an active share
  (or cancels an in-progress picker); never add a second stop button or a large
  status banner.
- The whiteboard toolbar is a native, compact toolbar within its own tile;
  changing the call view's appearance must not restyle its actions or canvas.

## Palette

Colors must derive from `QWidget::palette()` (Window, WindowText, Base,
Button, ButtonText, Mid, Highlight, HighlightedText) instead of a fixed
dark-on-dark or light-on-light CSS theme. Restrict Qt stylesheet selectors
to explicitly named `callView`, `callControls`, `callStage`, `callTile`
and individual call controls. **Never set global QLabel, QWidget or
QPushButton selectors on CallView**; such selectors leak into whiteboarding
and can produce white-on-white text. Semantic green/amber security indicators
and the red end-call action are the only fixed accent roles. Rebuild icons
and scoped styles in response to palette changes without recursive repolish.

## Behaviors worth regression testing

`src/avcall/unittest/screenshare.cpp` exercises the call view on Linux
with a mock ScreenCast portal. It should continue covering outgoing/incoming
peer visibility, microphone and camera signal semantics, real share-selection
cancel and stop, security accessibility descriptions, and dark theme palette
changes. Programmatic state updates must use signal blockers so toggling the
session's state from the backend does not emit a second user request.

The native window title is updated via the view's `headingChanged` signal;
keep this relationship for both editable and read-only peers.
