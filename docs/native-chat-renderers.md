# Native chat message renderers

Psi's native chat log (`MamChatWidget`) separates *what* a message consists of
from *how* it looks. Psi builds the semantic parts of every message and keeps
all of their behaviour; a renderer decides where the parts go and what
surrounds them. The built-in themes (Modern, Classic and Bubble) are renderers
written against the same public API that plugins use:
`plugins/include/nativechatrenderer.h`.

For customizing themes with a style sheet and for themes described without
code, see [native-chat-themes.md](native-chat-themes.md).

## Parts built by Psi

| Part        | Widget                                    | Present for                     |
|-------------|-------------------------------------------|---------------------------------|
| `Selection` | `QCheckBox` shown while selecting         | every row                       |
| `Avatar`    | `QLabel` (`mamAvatar`), square            | messages, if `avatarSize > 0`   |
| `Author`    | `QLabel` (`mamAuthor`)                    | messages                        |
| `Timestamp` | time label (`mamTimestamp`) with delivery, encryption and edit marks | messages |
| `Body`      | `mamBodyPart`: reply preview, text (`mamBody`), file cards, reactions | every row |
| `UnreadSeparator` | `QFrame#mamUnreadSeparator`, a theme-placeable unread boundary rule | only on the row immediately before unread messages |
| `DateSeparator` | `QLabel#mamDateSeparator`, localized calendar date | before the first row of each local calendar day |

Psi owns selection, hover highlighting and the hover actions toolbar, context
menus, link and media handling, reply resolution, relative-time refresh,
avatar loading, nickname colours, and the virtualization of the log. A renderer
never needs to reimplement any of it.

The body part reports the natural width of its content (text laid out without
wrapping, reply preview, file cards, reaction chips) as its size hint. With
`QSizePolicy::Preferred` and a maximum width, an ordinary Qt layout therefore
shrinks a container such as a bubble to short messages and wraps long ones.

## Writing a renderer

```cpp
class MyRenderer final : public NativeChatMessageRenderer {
public:
    NativeChatPartOptions partOptions(const NativeChatMessageInfo &info) const override
    {
        NativeChatPartOptions options;
        options.avatarSize = 32;
        options.timeFormat = QStringLiteral("HH:mm"); // Empty: "5 minutes ago".
        return options;
    }

    void arrange(NativeChatMessageCanvas &canvas, const NativeChatMessageInfo &info) const override
    {
        auto *outer = new QHBoxLayout(canvas.row());
        if (auto *selection = canvas.take(NativeChatMessageCanvas::Selection))
            outer->addWidget(selection, 0, Qt::AlignTop);
        auto *card = new MyCard(canvas.row()); // Any QWidget, may paint itself.
        auto *column = new QVBoxLayout(card);
        if (auto *author = canvas.take(NativeChatMessageCanvas::Author))
            column->addWidget(author);
        column->addWidget(canvas.take(NativeChatMessageCanvas::Body));
        outer->addWidget(card, 1, Qt::AlignTop);
        canvas.setActionAnchor(card);
    }
};
```

- `partOptions()` is asked before the parts are built. Besides the avatar size
  and the time format it sets the nickname suffix and weight and the
  timestamp's size, opacity and colour. Psi applies these when the row is
  built and again, in place, whenever the chat font, palette or nickname
  colours change.
- `header` decides where the time and the nickname go. `HeaderParts` (the
  default) builds the `Author` and `Timestamp` parts. `HeaderInText` puts
  them at the start of the message text, in one document with it, as in
  Psi's legacy text log: wrapped lines start at the left edge instead of
  under the end of the nickname. In the text, Psi puts the message icon
  (direction, delivery and encryption, following the "use message icons"
  option) before the time, wraps the nickname in `authorPrefix` and
  `authorSuffix`, makes it insert itself into the composer when clicked,
  and recolours it in place. The time is not refreshed there: give a clock
  `timeFormat`. Notices get an information icon instead.
- `directionColors` paints the nickname, in a conversation (not a group
  chat), in the colour of sent or received messages from Psi's options, as
  the legacy text log did; with the header in the text, the time too.
  Modern and Classic use it; Bubble, whose bubbles already tell sent from
  received, does not.
- `arrange()` gives `canvas.row()` a layout and places the parts taken with
  `take()`. Parts that are not taken are hidden. Rows are measured with
  `heightForWidth()`, so use layouts (or widgets) that support it.
- The optional `DateSeparator` is present on the first message of each
  local calendar day, including the first loaded row. The renderer can take
  the supplied `QLabel#mamDateSeparator` and place it anywhere above the
  message. Without `take(DateSeparator)`, Psi wraps the renderer layout
  and shows a centred date with horizontal rules before the content.
  The separator is part of the message row, so virtual history paging and
  selection identities are unchanged. When older messages arrive, Psi
  refreshes the following row to avoid duplicate dividers.
- The optional `UnreadSeparator` is present only on the row marking the
  unread boundary. The renderer may take and arrange the supplied
  `QFrame#mamUnreadSeparator` (and style it via QSS), or hide it to draw
  a completely custom indicator. If the renderer does not take it, Psi
  wraps the renderer's existing row layout and places that same frame
  beneath the message content. There is no separately painted fallback
  rule. This extends the existing `Part` enum, not the renderer's vtable,
  without changing the renderer's vtable.
  `setActionAnchor()` attaches the hover actions toolbar to a widget other
  than the row, and `setBodyTextColor()` overrides the text colour of the body,
  e.g. on a dark bubble.
- `restyle()` is optional. Psi calls it after restyling the parts of an
  existing row in place. Widgets that read `palette()` and `font()` while
  painting need nothing here.
- A row is built again when its message or state changes, or when the theme
  changes. Psi never calls a renderer for rows built by another renderer.
- `NativeChatMessageInfo` says what is being rendered: the kind (message,
  notice or subject), whether it is our own message, a group chat, an emote or
  retracted, the sender's nickname and the time.

The row itself is Psi's `mamMessageRow` frame. Psi paints its hover/selection
surface and applies the theme's style sheet to it; keep it as the root of the
renderer's widgets.

## Plugins

A plugin implements `NativeChatThemeProvider` and returns complete
`NativeChatTheme` objects. Each theme optionally owns a
`NativeChatMessageRenderer` through `setRenderer()`; there is no separate
renderer-provider interface or registry lookup. The caller uses
`theme.renderer()`, which returns the supplied renderer or creates a generic
description/JSON renderer.

```cpp
class MyPlugin : public QObject, public PsiPlugin, public NativeChatThemeProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.psi-plus.MyPlugin" FILE "psiplugin.json")
    Q_INTERFACES(PsiPlugin NativeChatThemeProvider)
public:
    QList<NativeChatTheme> nativeChatThemes() const override
    {
        NativeChatTheme cards(QStringLiteral("org.example.cards"), tr("Cards"), 32, 8);
        cards.setRenderer(std::make_shared<MyRenderer>());
        return { cards };
    }
};
```

The theme provider IID is `org.psi-im.NativeChatThemeProvider/2.0`. No
pre-existing third-party native-theme plugins require compatibility with the
earlier provisional interface.

### Lifetime

A `NativeChatTheme` copy retains a shared handle to its renderer. Rows built
by that renderer may also contain plugin widget classes and lambdas. When a
plugin is disabled, Psi removes its registered themes and rebuilds the visible
rows with the fallback theme. Deferred widget deletion can outlive disabling,
so any plugin contributing a C++ renderer remains loaded until Psi exits;
description-only plugins do not have that restriction.

## Built-in themes

- **Modern**: avatar, a bold nickname with a smaller relative time, and the
  body below them.
- **Classic**: the legacy text log, `[HH:mm:ss] <nick> text` in one document
  (`HeaderInText`), with the legacy message icons before the time, the
  time painted in the nickname colour in group chats, and notices left
  aligned after an information icon.
- **Bubble**: places the date in a centred bubble without a tail, above the
  first message of a local calendar day. Its own date bubble has the property
  `dateSeparator=true` and its own `mamDateBubble` object name for styling. It places its unread separator
  in a separate region beneath the bubble
  rather than drawing a line over the message. Each message is a speech bubble whose text area
  is limited to about 60 average characters of the chat font (by default),
  whatever the width of the log, and which shrinks to short messages. Incoming messages sit
  on the left and our own on the right, each with the avatar on its outer side
  and the bubble's tail near its top pointing at the avatar. Notices and the
  group chat subject are centred bubbles without a tail, as in the WebKit
  Bubble theme. The message under the pointer is not tinted: the bubbles
  already set each message apart. Style sheets restyle the bubbles through
  `#mamBubble[tone=...]` and its `fillColor`, `borderColor`, `radius` and
  `textColumns` properties.

Built-in themes live in `src/widgets/nativechat/themes/`: each theme has
its own source file, and `BuiltinThemes.cpp` is the single registration list.
Built-ins use the same `NativeChatThemeProvider` interface as plugins and
register through `NativeChatThemes::add()`. Each of Modern, Classic and Bubble
owns its own C++ renderer in its respective source file; the
`nativechat/DescriptionRenderer.cpp` interpreter handles only themes without
a C++ renderer, using `layoutSpec` or a neutral default layout. The registry
does not know or special-case any theme ID.

## Style sheets

Rows built by a renderer are restyled by the chat window's style sheet
(`options.ui.chat.css`) and the theme's `styleSheet` like any other. A rule
that colours a part wins over the colours Psi computes for it, also after
Psi recomputes them. Give the widgets a renderer creates object names and
dynamic properties, and declare `Q_PROPERTY`s for what it paints itself, so
users can restyle them with selectors and `qproperty-` declarations.
