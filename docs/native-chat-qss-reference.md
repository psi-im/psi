# Native chat log: QSS styling reference

This is an element-by-element guide for styling Psi's **native** chat log (`MamChatWidget`) in the current Qt5/Qt6 implementation. It complements [native-chat-themes.md](native-chat-themes.md) (options and theme plugins) and [native-chat-renderers.md](native-chat-renderers.md) (renderer API).

**This is Qt Style Sheets (QSS), not browser CSS.** There is no DOM: a `QTextEdit` contains a `QTextDocument`, and individual HTML spans/anchors inside it are not addressable through QWidget selectors. Some things (notably code-block surfaces) are drawn with `QPainter` rather than QSS.

## Where to enter styles

1. Go to **Options → Advanced** and edit `options.ui.chat.css`.
2. **Append** your QSS rules: the default value already contains a correction-highlight rule for the message composer.
3. Press **Apply** or **OK**. Reopen the conversation if a style/property appears stale. Clearing a `qproperty-` override can require reopening the chat.
4. Use the [Qt Style Sheet reference](https://doc.qt.io/qt-6/stylesheet-reference.html) for the supported syntax. Not every web CSS property, pseudo-class or selector is supported.

This option applies to normal chat and groupchat windows and their native log, independent of whether the built-in theme is Classic, Modern or Bubble. A theme plugin can supply `NativeChatTheme::styleSheet` **on each row**, closer to the message widgets than the window-level style sheet; conflicting theme rules then take precedence.

Style **chat-wide defaults** (body and nickname colours, chat background, links, quote and informational colours, chat font) in **Options → Appearance** when you want them to follow the system palette. Fixed QSS colours will not automatically adapt to dark/light mode.

## Selector quick reference

`#name` means a QWidget `objectName`, not an HTML ID. The selector `QLabel#mamAuthor` targets that named label, and `#mamBubble[tone="own"]` selects a widget with the dynamic property `tone`. Parts are created only when used by the active theme and/or message.

### Log and message row

| Selector | Meaning | What can be styled |
| --- | --- | --- |
| `MamChatWidget` | The complete scrolling native chat log | Use its `qproperty-` settings below for background and hover |
| `#mamMessageRow` | Root of each materialized message | Use as an **ancestor selector**, e.g. `#mamMessageRow[alert="true"] QTextEdit#mamBody`; **do not** set a row background |
| `#mamMessageRow[alert="true"]` | An alerted message (MUC mention or recognized reply/quote to you) | Scope child rules to alerts; Bubble also has `tone="alert"` |
| `QCheckBox#mamMessageSelection` | Checkbox in whole-message selection mode | Normal QSS checkbox styling, including `::indicator` |
| `QFrame#mamSelectionToolbar` | Floating bar used during multi-message selection | Frame background, border, padding |
| `QPushButton#mamCopySelected` | Copy button on that selection bar | Normal button QSS |

The message-row surface, hover tint and selected-row tint are painted by the timeline, not by the row's stylesheet. Styling `#mamMessageRow { background: ... }` can interfere with those states. There is **no public `own`/`incoming` row property** for Modern or Classic; only Bubble's bubble has a documented `tone`.

### Sender, time, text and status

| Selector | Meaning | Availability / caveat |
| --- | --- | --- |
| `QLabel#mamAvatar` | Sender avatar | Modern/Bubble messages when avatars are enabled; the actual image/size comes from Psi/theme |
| `QLabel#mamAuthor` | Sender nickname | Modern/Bubble (not Classic); `color`, `font-weight`, `font-style` etc. |
| `QLabel#mamTimestamp` | Message time | Modern/Bubble (not Classic); relative time changes dynamically |
| `#mamTimestampGroup` | Container for time and status marks | Created only if delivery/read, encryption or edit status is present |
| `QLabel#mamDeliveryIndicator[deliveryState="delivered"]` | Single delivery check | Own private messages with delivery state |
| `QLabel#mamDeliveryIndicator[deliveryState="read"]` | Read/double check | Own private messages with read marker |
| `QLabel#mamEncryptionIndicator[encrypted="true"]` | Encrypted-message icon | Only when encryption state is known |
| `QLabel#mamEncryptionIndicator[encrypted="false"]` | Not-encrypted icon | Only when encryption state is known |
| `QLabel#mamEditedIndicator` | Edit/pencil mark | Only when the message is marked edited |
| `#mamHeader`, `#mamHeaderGroup` | Header layout containers | Present only where the renderer builds them; padding and surrounding style |
| `#mamBodyPart` | Container holding the body and related parts | Useful as an ancestor selector |
| `QTextEdit#mamBody` | Message prose, including prose **before and after code fences** | Base font, colour, selection and text-edit chrome; rich text inside may have its own formatting |

**Status-icon caveat:** delivery checks are painted from a palette colour, so `color` on `#mamDeliveryIndicator` is meaningful. Encryption and edited marks are **pixmaps from Psi's iconset**. QSS `color` does not recolour those actual pixels. For icon colour/shape, change the iconset or add a renderer/API feature.

**Classic caveat:** Classic uses `HeaderInText`: direction icon, bracketed time, nickname and message share **one** rich-text document, `QTextEdit#mamBody`. It creates no separate `mamAuthor`/`mamTimestamp` label (nor a timestamp-mark group). There is no Qt-widget QSS selector for its inline nickname or time. Their colours come from Psi's chat colours and QTextDocument formatting. Use the appearance colour options or a theme/renderer change for separate styling.

Likewise, **links, inline code, bold/italic, quoted lines, inline images and markup inside a prose document** do not become named child widgets. `QTextEdit#mamBody a` or `.quote` are not DOM/CSS selectors in QSS. Use Psi's colour options and rich-text rendering code for these.

### Reply/quote preview and file preview

The same `MessagePreview` widget is used for a reply to another message and for file cards; its root gets one of two names:

| Selector | Meaning | Styling |
| --- | --- | --- |
| `QFrame#mamReplyPreview` | Inline reply/quoted-message card | Background, border, padding; exists only for valid replies |
| `#mamReplyPreview[replyResolved="true"]` | Reply found in the available history | Scope resolved-quote styling |
| `#mamReplyPreview[replyResolved="false"]` | Reply not resolved locally | Scope unresolved-quote styling |
| `QFrame#mamFilePreview` | File card (when media is not represented inline) | Card background, border, padding |
| `#mamReplyPreview #previewBar` | Vertical colour bar in reply | Frame background/border; quote colour is the default |
| `#mamFilePreview #previewBar` | Vertical bar in a file card | Same element, different card |
| `#previewTitle` | Sender/title for a quoted message, or filename in a file card | QLabel text colour/font |
| `#previewTimestamp` | Time belonging to the **quoted original** | QLabel text colour/font; may be absent/hidden |
| `#previewContent` | Preview body container | Background/padding if useful |
| `QTextEdit#previewText` | Quoted/file-preview text | Base text colour/font; same QTextDocument limitations as `mamBody` |
| `#previewClip` | Clipping viewport for collapsed preview | Structural widget; not the text |
| `#previewShowMore` | Show more/less interactive text | Custom-painted `LinkButton`; see below |
| `QPushButton#previewDismiss` | Close/cancel reply button | Present where a preview is closable |
| `QLabel#mamFileStatus` | File-card download progress, path or error | Text colour/font/weight |
| `#mamFilePreview QToolButton` | **All** file-card control buttons | General buttons only; download/open/reveal/copy/cancel currently have **no distinct object names** |

`#previewBar`, `#previewTitle`, `#previewTimestamp` and `#previewText` repeat across preview instances. Scope them by `#mamReplyPreview` or `#mamFilePreview` to colour replies and files independently.

`#previewShowMore` is a `LinkButton`, which paints its own text with a `linkColor` Q_PROPERTY. Use `qproperty-linkColor` rather than relying on the usual button `color`. Psi also sets link colour programmatically when creating previews, so retest after palette/theme changes.

### Reaction chips and hover toolbar

| Selector | Meaning | Caveat |
| --- | --- | --- |
| `#mamReactions` | Container of the reactions under a message | Structural widget |
| `QPushButton#mamReactionChip` | Each emoji+count chip | `:checked` means **our own** reaction; `:hover` is supported |
| `QFrame#mamMessageActions` | Floating hover actions bar | Background and border are already set by its **own** style sheet |
| `#mamQuickReactions` | Expandable quick-reaction area | Layout container |
| `QPushButton#mamQuickReaction` | Quick emoji buttons | All quick emojis share one object name |
| `QPushButton#mamChooseReaction` | Emoji picker button | Standard button QSS |
| `QPushButton#mamMoreActions` | More actions button | Standard button QSS |
| `QMenu#mamMessageMenu` | Hover toolbar's popup menu | Normal `QMenu` selectors such as `::item` |

**Priority limitation:** each reaction chip has its own `setStyleSheet()` (normal, hover, checked and checked+hover), and the floating `mamMessageActions` frame also has a local style sheet. Those local declarations beat conflicting rules placed at the chat-window level. To give users *full* QSS control, these hard-coded sheets should be removed or made configurable.

`QAction` names such as `mamRetractMessage` are **action identifiers, not QWidget selectors**. Styling `QAction#mamRetractMessage` will not recolour a menu item; QSS styles the owning `QMenu`/`::item`.

### Dates, unread separator and notices

| Selector | Meaning | Availability |
| --- | --- | --- |
| `QLabel#mamDateSeparator` | Human-readable local calendar date | First message of each day, if valid |
| `#mamDateDivider` | Default date-separator region | When a renderer does not consume the date part |
| `QFrame#mamDateDividerRule` | Horizontal line(s) around default date text | When default divider is used |
| `#mamDateBubble` | **Bubble** theme's centred date bubble | Separate `BubbleFrame` with its own colour/radius properties |
| `#mamBubbleDateRow` | Bubble date row | Bubble only |
| `QFrame#mamUnreadSeparator` | Unread boundary line | Only on the boundary row; position chosen by renderer/fallback |
| `#mamBubbleUnreadDivider` | Wrapper below a bubble for the unread line | Bubble only |
| `QLabel#mamNoticeHoverTimestamp` | Hover-only `HH:mm` beside notices (`/me`, join/part) | Bubble or an opt-in renderer; clock visibility is animated, not styled by `:hover` |

Notice/subject messages are centred tailless `#mamBubble[tone="notice"]` in Bubble. In Classic they are inline text with a left-aligned information icon. Standard QLabel author/time selectors do not appear for notices.

### Bubble-only surfaces

Bubble's `#mamBubble` is a `BubbleFrame` with **custom QPainter rendering**. Ordinary `background-color`/`border-radius` on its QWidget does not control the actual painted speech-bubble shape. Use these Q_PROPERTY declarations:

| Selector/property | Use |
| --- | --- |
| `#mamBubble[tone="incoming"]` | Normal incoming bubble |
| `#mamBubble[tone="own"]` | Normal outgoing bubble |
| `#mamBubble[tone="alert"]` | Alert bubble (takes precedence over own/incoming) |
| `#mamBubble[tone="notice"]` | Notice/subject bubble |
| `#mamBubble[tailSide="left"]` | Tail on left |
| `#mamBubble[tailSide="right"]` | Tail on right |
| `#mamBubble[tailSide="none"]` | Tailless |
| `qproperty-fillColor` | Painted bubble fill, e.g. `#f3f6fb` |
| `qproperty-borderColor` | Painted border; `transparent` removes it |
| `qproperty-radius` | Corner radius (clamped to 0–30) |
| `qproperty-textColumns` | Maximum bubble width in average chat-font characters (clamped to 10–1000; default 60) |

The date bubble (`#mamDateBubble`) uses the same Q_PROPERTY names **independently**. To style only an outgoing message's time, use `#mamBubble[tone="own"] QLabel#mamTimestamp`.

### Fenced code blocks (partial QSS support)

| Selector | Meaning | Current reality |
| --- | --- | --- |
| `#mamCodeBlock` | Whole fenced code widget | Outer rounded background and border are hard-coded in its `paintEvent()`; QSS `background` does **not** recolour that paint |
| `#mamCodeBlockHeader` | Header strip | QWidget; can accept QSS, but may not change the parent-painted region |
| `#mamCodeBlockBody` | Editor's surrounding area | QWidget |
| `QPlainTextEdit#mamCodeBlockEditor` | Read-only code editor | Palette and local `setStyleSheet()` are applied by code; syntax highlighting uses QSourceHighlite |
| `QToolButton#mamCodeBlockLanguage` | Language selector | Local QSS overrides the usual chat-window QSS |
| `QToolButton#mamCodeBlockAction` | Action buttons (copy, wrap, open, format, run/plugin actions) | **All share the same object name** and local QSS; cannot style per action with window QSS |
| `QMenu#mamCodeLanguageMenu` | Syntax-language menu | Menu QSS, not code-lexer colours |
| `#mamCodeBlockViewer` | Separate code-viewer window | A separate window: do not assume it inherits chat-window QSS |

Action objects have names such as `mamCodeBlockCopy`, `mamCodeBlockWordWrap`, `mamCodeBlockOpenWindow`, `mamCodeBlockFormat`, `mamCodeBlockRun`, but **QAction is not the rendered QToolButton**. Selector rules against those names do not individually recolour their buttons.

The current Monokai-like code surface, border and syntax-token colours are **not fully configurable through `options.ui.chat.css`**. A true per-token colour scheme needs a highlighter/style API, not CSS selectors pretending that tokens are DOM nodes.

## Chat-log-specific Q_PROPERTY values

Use these on `MamChatWidget` itself, **not** on the virtual message row:

| Declaration | Meaning |
| --- | --- |
| `qproperty-hoverHighlight: false;` | Disable hovered-row tint (off by default in Classic/Bubble) |
| `qproperty-backgroundColor: #f5f1e9;` | Solid chat background; overrides theme and appearance background |
| `qproperty-backgroundImage: url(/absolute/path/to/paper.png);` | Wallpaper; Qt resource paths are also supported |
| `qproperty-backgroundImage: "none";` | Remove the theme's wallpaper |
| `qproperty-backgroundImageMode: tile;` | Tile background picture (default) |
| `qproperty-backgroundImageMode: scale;` | Cover background while keeping aspect ratio |

When wallpaper is used, message rows stay transparent unless hovered/selected. Fixed background and bubble colours need matching readable foreground colours.

## Copy/paste examples

Append these examples to `options.ui.chat.css` **only after choosing colours appropriate to your palette**:

```css
/* 1. All themes: background, normal prose, sender and time. */
MamChatWidget {
    qproperty-backgroundColor: #f7f7fa;
    qproperty-hoverHighlight: false;
}
QTextEdit#mamBody { color: #20242b; }
QLabel#mamAuthor { color: #385f93; font-weight: bold; }
QLabel#mamTimestamp, QLabel#mamNoticeHoverTimestamp {
    color: #6d7582; font-size: 9pt;
}
/* 2. Delivery states; icons use pixmaps and are not recoloured by color. */
QLabel#mamDeliveryIndicator[deliveryState="delivered"] { color: #808b98; }
QLabel#mamDeliveryIndicator[deliveryState="read"] { color: #1787cb; }
/* 3. Alerts: target body descendants, not the row background. */
#mamMessageRow[alert="true"] QTextEdit#mamBody {
    color: #ad2433; font-weight: bold;
}
/* 4. Reply and file previews use the SAME internal preview parts. */
#mamReplyPreview QFrame#previewBar { background-color: #b967a0; border: none; }
#mamReplyPreview QLabel#previewTitle { color: #9a3c79; font-weight: bold; }
#mamReplyPreview QLabel#previewTimestamp { color: #817684; }
#mamReplyPreview QTextEdit#previewText { color: #694d62; }
#mamReplyPreview #previewShowMore { qproperty-linkColor: #ad4a84; }
#mamFilePreview QFrame#previewBar { background-color: #5295ba; border: none; }
#mamFilePreview QLabel#previewTitle { color: #326e97; font-weight: bold; }
QLabel#mamFileStatus { color: #697687; }
/* 5. Calendar day and unread divider. */
QLabel#mamDateSeparator { color: #557188; font-weight: bold; }
QFrame#mamUnreadSeparator { color: #c75058; }
/* 6. Bubble theme: colour each message category independently. */
#mamBubble[tone="incoming"] {
    qproperty-fillColor: #e8ecf2;
    qproperty-borderColor: #cad2de;
}
#mamBubble[tone="own"] {
    qproperty-fillColor: #d9f2df;
    qproperty-borderColor: transparent;
}
#mamBubble[tone="alert"] {
    qproperty-fillColor: #ffe1e1;
    qproperty-borderColor: #cf5960;
}
#mamBubble[tone="notice"] {
    qproperty-fillColor: #f0ecdc;
    qproperty-radius: 4;
}
#mamBubble { qproperty-textColumns: 68; }
#mamDateBubble {
    qproperty-fillColor: #f0ede5;
    qproperty-borderColor: transparent;
    qproperty-radius: 12;
}
#mamBubble[tone="own"] QLabel#mamTimestamp { color: #587b63; }
/* 7. The buttons below demonstrate selectors, but existing local
   setStyleSheet() on reaction chips/toolbar takes precedence. */
QPushButton#mamReactionChip:checked { border-color: #3a9d62; }
QPushButton#mamChooseReaction:hover { background: #e7ebf2; }
```

The example is illustrative, not a drop-in dark/light adaptive palette. Test in a private chat and a MUC, with incoming, outgoing, edited, encrypted, read, alert, reply, file, code, notice, date change, reactions and unread boundary.

## What cannot currently be independently recoloured using only QSS

1. **Classic's inline nickname/time/direction icons**: they live inside a QTextDocument, not standalone QLabel widgets.
2. **Rich-text spans and tokens** (URLs, XEP-0393 quote blocks, inline emphasis, syntax-highlighted source): they do not have QWidget selectors. Change Psi's rich-text/highlighter code or the relevant appearance option.
3. **Actual pixel colours of avatars, images, encryption/edit icons**: images/iconset determine them, not a QLabel `color` rule.
4. **Code-block rounded frame and lexer tokens**: code paints the surface and chooses syntax colours directly.
5. **Per-action code buttons**: every button is named `mamCodeBlockAction`, with locally set QSS; QAction object names are not CSS targets.
6. **Per-function file buttons** (download/open/reveal/copy/cancel): currently unnamed; all are `QToolButton` descendants of `mamFilePreview`.
7. **User-recolourable reaction-chip and hover-toolbar declarations**: their local style sheets override the window-level ones.
8. **Message layout, widths/alignment and part order**: defined by the renderer/theme (Bubble exposes only the documented `textColumns` width control), not by CSS.

If the goal is genuinely **every element independently**, the above are styling-API gaps to fix in Psi: give all buttons stable object names/properties; stop hard-coding local QSS where user QSS should override it; expose code block surface colours as Q_PROPERTY; give Classic richer semantic styling hooks; and expose syntax palettes separately.

## Source of truth

- `src/widgets/MamChatWidget.cpp` and `src/widgets/MamChatWidget.h` — widget creation, dynamic properties, log properties, reactions/actions.
- `src/widgets/MessagePreview.cpp` — reply and file-preview internals.
- `src/widgets/CodeBlockWidget.cpp` — code widgets and painted surface.
- `src/widgets/ChatMessageBody.cpp` — text/code splitting.
- `src/widgets/nativechat/themes/Bubble.cpp`, `Modern.cpp`, `Classic.cpp` — theme-specific layouts.
- `src/widgets/LinkButton.cpp` — painted show-more text.
- [Theme customization and plugins](native-chat-themes.md) and [renderer API](native-chat-renderers.md).

This list describes the **current implementation** rather than an HTML/CSS contract. Third-party renderer plugins may add their own names and properties.
