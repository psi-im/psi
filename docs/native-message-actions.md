# Native message actions and replies

`MamChatWidget` owns presentation and stable row keys. Context-menu actions resolve
those keys again when activated, so prepending history or evicting a row widget
cannot redirect an action to another message. Copy with a text selection uses
that text view, including selections in quoted excerpts. Otherwise it uses the
same sender/timestamp/body formatter as whole-message copying.

MUC nicknames and avatars delegate to `GCMainDlg`'s existing roster menu.
Departed participants get nickname actions and, when presence disclosed it,
actions for the last known real JID. The bounded, session-only address cache is
cleared when a nickname is reused; it does not infer a real JID from an occupant
address or a message body.

Replies use XEP-0461 and Iris's typed reply/fallback API. Native MUC replies need
the room-assigned stanza ID, while direct chats use origin-id or message@id.
`ChatEditProxy` owns reply context and its read-only composer preview independently
of the replaceable editor. Sending consumes that context; cancellation preserves
the draft text. Compatibility fallbacks count Unicode code points. They are
omitted for messages with other reference/fallback ranges to leave those ranges
intact. Recent outgoing corrections retain their original reply reference.

`MessagePreview` is a generic selectable title/excerpt card. Show more expands it
within a window-relative height limit, then enables a scrollbar. It does not own
network fetching or modify the editor. Its constructor accepts a vertical-line
color and an optional text color; `setColors()` updates those without resetting
expansion or text selection. An absent text color leaves Qt palette inheritance
intact, so a link preview can use a link-colored line with ordinary text. Quote
callers pass `messages.quote` from the current light/dark desktop palette for
both colors, including in the composer. This color is editable in Appearance.
Show more/less uses `LinkButton`, a text-only `QAbstractButton` with no native
button painting or padding. Callers pass the configured `messages.link` color
separately, including changes on an existing card. Its text uses 80% opacity,
100% on hover; normal button activation and keyboard access remain available.
Native received replies resolve a local
target using both its ID and author; unresolved targets display their explicit
fallback, without guessing from an arbitrary quoted prefix. XHTML and plugin
rendering are preserved when removing identifiable marked fallback ranges.

EDB keeps reply metadata alongside fallbacks in `events.extra_data`. This is an
additive JSON extension; existing rows and the SQLite schema need no migration.
Old history without a structured reference remains ordinary text.
