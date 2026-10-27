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

A plugin declares its themes with `NativeChatThemeProvider` and offers
renderers for them with `NativeChatRendererProvider`; both go into
`Q_INTERFACES`. Themes without a renderer are laid out from their description
(base layout, avatar size, spacing, style sheet, optional JSON `layoutSpec`).

```cpp
class MyPlugin : public QObject, public PsiPlugin, public NativeChatThemeProvider,
                 public NativeChatRendererProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.psi-plus.MyPlugin" FILE "psiplugin.json")
    Q_INTERFACES(PsiPlugin NativeChatThemeProvider NativeChatRendererProvider)
public:
    QList<NativeChatTheme> nativeChatThemes() const override
    {
        return { { QStringLiteral("org.example.cards"), tr("Cards"), NativeChatTheme::Modern, 32, 8, {}, {} } };
    }
    const NativeChatMessageRenderer *nativeChatRenderer(const QString &id) const override
    {
        return id == QLatin1String("org.example.cards") ? &renderer_ : nullptr;
    }

private:
    MyRenderer renderer_;
};
```

The renderer interface is `org.psi-im.NativeChatRendererProvider/1.1`;
plugins built against 1.0, before the header options existed, must be
rebuilt.

### Lifetime

Rows built by a renderer contain the plugin's code: its widget classes, and any
lambdas it connected. When such a plugin is disabled, Psi removes its themes,
rebuilds the open chat logs with a built-in theme and stops calling the
renderer, but the old rows are destroyed asynchronously. A plugin that
registered a renderer is therefore never unloaded from memory at runtime: it
is disabled, and its library stays loaded until Psi exits. The renderer object
must stay valid until the plugin object is destroyed, not only until
`disable()`.

## Built-in themes

- **Modern**: avatar, a bold nickname with a smaller relative time, and the
  body below them.
- **Classic**: the legacy text log, `[HH:mm:ss] <nick> text` in one document
  (`HeaderInText`), with the legacy message icons before the time, the
  time painted in the nickname colour in group chats, and notices left
  aligned after an information icon.
- **Bubble**: based on Modern. Each message is a speech bubble whose text area
  is limited to about 60 average characters of the chat font (by default),
  whatever the width of the log, and which shrinks to short messages. Incoming messages sit
  on the left and our own on the right, each with the avatar on its outer side
  and the bubble's tail near its top pointing at the avatar. Notices and the
  group chat subject are centred bubbles without a tail, as in the WebKit
  Bubble theme. The message under the pointer is not tinted: the bubbles
  already set each message apart. Style sheets restyle the bubbles through
  `#mamBubble[tone=...]` and its `fillColor`, `borderColor`, `radius` and
  `textColumns` properties.

The renderers are in `src/widgets/NativeChatRenderers.cpp`.

## Style sheets

Rows built by a renderer are restyled by the chat window's style sheet
(`options.ui.chat.css`) and the theme's `styleSheet` like any other. A rule
that colours a part wins over the colours Psi computes for it, also after
Psi recomputes them. Give the widgets a renderer creates object names and
dynamic properties, and declare `Q_PROPERTY`s for what it paints itself, so
users can restyle them with selectors and `qproperty-` declarations.
