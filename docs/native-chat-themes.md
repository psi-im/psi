# Native chat themes: customizing and writing them

This guide is for users who want to adjust how the native chat log looks and
for developers who want to ship their own chat themes as Psi plugins.

There are four levels, from the simplest to the most powerful:

1. **Options.** The theme, the chat font and the chat colours.
2. **A style sheet** (`options.ui.chat.css`). Tweaks any theme, built-in or
   not, without writing code.
3. **A theme plugin with a description.** A new entry in the theme list,
   with your own avatar size, spacing, style
   sheet and part arrangement. No widget code.
4. **A theme plugin with a renderer.** Full control over the widgets around
   the parts of a message, e.g. Psi's own Bubble theme in
   `src/widgets/nativechat/themes/Bubble.cpp`. See
   [native-chat-renderers.md](native-chat-renderers.md).

## 1. Options

- **Theme:** Options → Appearance → Themes → *Chat Message Style* and
  *Groupchat Message Style* (`options.ui.chat.native-theme`,
  `options.ui.muc.native-theme`). The built-in themes are `modern`,
  `classic` and `bubble`.
- **Font:** the chat font (`options.ui.look.font.chat`). Every theme scales
  its parts from it.
- **Colours:** Options → Appearance → Colors: chat background and text,
  links, informational messages, quotes and nickname colours
  (`options.ui.look.colors.*`). Themes derive their colours from these: the
  bubbles, for instance, are tinted from the chat background, text and
  highlight colours, so a theme follows light and dark palettes by itself.

Prefer these over a style sheet for colours that should apply everywhere.

## 2. Customizing a theme with a style sheet

For an element-by-element selector table, copy/paste examples and an explicit list
of current QSS limitations, see [native-chat-qss-reference.md](native-chat-qss-reference.md).

`options.ui.chat.css` is a Qt style sheet
([QSS reference](https://doc.qt.io/qt-6/stylesheet-reference.html)) that Psi
sets on chat and group chat windows. Edit it in Options → Advanced. Changes
apply when you press *Apply* or *OK* in the Options dialog and to every newly
opened chat window. The default value already contains a rule for the message
editor, so append your rules rather than replacing it. Clearing the option
takes effect in newly opened windows only.

Rules reach every message in the log, whichever theme is selected. The
selectors below are stable names Psi gives to the parts of a message. The
Classic theme has the time and the nickname in the message text, so it has
no `mamAuthor` or `mamTimestamp` labels; style its text through
`QTextEdit#mamBody`.

### Selectors

| Selector | What it is | Notes |
|----------|------------|-------|
| `QLabel#mamAuthor` | Sender's nickname | Colour, font, padding, border, background. |
| `QLabel#mamTimestamp` | Time | Colour, font, padding. |
| `QLabel#mamDeliveryIndicator` | Delivery and read checks | `color` only. `[deliveryState="delivered"]`, `[deliveryState="read"]`. |
| `QLabel#mamEncryptionIndicator`, `QLabel#mamEditedIndicator` | Lock and pencil icons | Icons; `[encrypted="true"]`/`"false"` on the lock. |
| `QLabel#mamAvatar` | Avatar picture | Picture and size come from Psi and the theme. |
| `QTextEdit#mamBody` | Message text, including each piece around a code block | Colour, font, padding, border. |
| `#mamReplyPreview` | Quoted message above the text | `[replyResolved="true"]` when the quoted message was found. Its colours come from the quote colour option. |
| `#mamFilePreview` | File and media cards | |
| `#mamBubble` | A speech bubble (Bubble theme) | `[tone="incoming"]`, `[tone="own"]`, `[tone="notice"]`; `[tailSide="left"]`, `"right"`, `"none"`. Painted by Psi, styled with the properties below. |
| `#mamHeader`, `#mamHeaderGroup` | Container of nickname and time | Spacing via padding. |
| `#mamMessageRow` | The whole message row | Use only as an ancestor in selectors, see *Limitations*. `[alert="true"]` includes MUC mentions and replies to your messages. |
| `QFrame#mamUnreadSeparator` | Unread boundary when the renderer places it | Style its border/colour/height in QSS; absent on other messages. |
| `QLabel#mamDateSeparator` | Local calendar date divider | Theme-placeable date text, styled like other labels. |
| `#mamDateBubble` | Bubble theme's date bubble | Set `qproperty-fillColor`, `qproperty-borderColor` and `qproperty-radius` independently of message bubbles. |

Combine selectors to style parts in one kind of message, for example our own
messages in the Bubble theme: `#mamBubble[tone="own"] QLabel#mamTimestamp`.

### Unread boundary

The native timeline stores the unread boundary independently of the
materialized message widgets. It creates a single `QFrame#mamUnreadSeparator`
for the boundary row. A renderer can take the optional
`NativeChatMessageCanvas::UnreadSeparator` part and place it anywhere in
its layout; otherwise Psi puts the same frame below the renderer's content.
The built-in Bubble renderer lays it out below the bubble.

A description-only theme may add `"unreadSeparator"` to its JSON
`layoutSpec`, for example:
```json
{"direction":"vertical","items":["body","unreadSeparator"]}
```
The part is a `QFrame#mamUnreadSeparator` that can be customized with QSS.
Themes that do not take the part receive the same frame beneath their message
layout automatically.

### Join/part hover time

Bubble shows a hover-only `HH:mm` next to join/part and `/me` notices.
Classic and Modern keep their existing presentation. Declarative themes can
opt in with `"noticeTimestamp"` in their JSON `layoutSpec`.

### Date dividers

Psi supplies `NativeChatMessageCanvas::DateSeparator` only for the first
message of a local calendar day. The default placement is a centred,
localized date with thin lines on both sides, above the message. The Bubble
theme places the same label inside its own centred, tailless date bubble.
Its foreground follows the informational-message color (`messages.informational`) from
Appearance → Colors, with independent light and dark palette defaults. Changing
that setting recolors visible dates without rebuilding message rows.
Themes may style `QLabel#mamDateSeparator` or consume the part and arrange
it themselves. For description themes, add `"dateSeparator"` to the
`layoutSpec` wherever the date should appear (typically before `"body"`).
Dividers are part of the original message rows, not separate virtual list
items, so appending and prepending history do not disturb navigation.

### Bubble properties

The Bubble theme paints its bubbles itself. Style sheets set their look with
`qproperty-` declarations:

| Property | Default | Meaning |
|----------|---------|---------|
| `qproperty-fillColor` | from the palette | Bubble background. |
| `qproperty-borderColor` | from the palette | Outline; `transparent` for none. |
| `qproperty-radius` | `10` | Corner radius in pixels (0–30). |
| `qproperty-textColumns` | `60` | Maximum text width in average characters of the chat font. |

Fixed colours do not follow a switch between light and dark palettes. Use
`palette(...)` values where possible, or give your own and incoming bubbles
colours that work with the chat background you use.

### The chat log

The log itself is `MamChatWidget`. Its background and hover behaviour come
from the theme (section 3); these properties override the theme, also after
a theme switch:

| Property | Meaning |
|----------|---------|
| `qproperty-hoverHighlight` | `true` or `false`: tint the message under the pointer. Off in Bubble. |
| `qproperty-backgroundColor` | Background colour instead of the chat background colour option. |
| `qproperty-backgroundImage` | `url(/path/to/image.png)` or a resource `url(:/...)`, fixed behind the scrolling messages; `"none"` (quoted) drops the theme's image. |
| `qproperty-backgroundImageMode` | `tile` (default) or `scale`, which covers the log and keeps the aspect ratio. |

Over an image, rows are transparent except while the pointer is over them or
they are selected. The chat background colour still matters: Bubble tints its
bubbles from it, and it shows through transparent parts of the image.

### Examples

```css
/* Smaller, grey times and italic nicknames in every theme. */
QLabel#mamTimestamp { color: rgba(128, 128, 128, 200); font-size: 8pt; }
QLabel#mamAuthor { font-style: italic; }

/* A declarative theme can style mentions and replies to your messages. */
#mamMessageRow[alert="true"] QTextEdit#mamBody {
  color: #b9232d;
  font-weight: bold;
}

/* Blue checks once a message has been read. */
QLabel#mamDeliveryIndicator[deliveryState="read"] { color: #3d9be9; }

/* Bubble: green own bubbles without an outline, wider text. */
#mamBubble[tone="own"] {
    qproperty-fillColor: #dcf8c6;
    qproperty-borderColor: transparent;
}
#mamBubble { qproperty-textColumns: 75; }
#mamBubble[tone="own"] QTextEdit#mamBody { color: #1b3d0f; }

/* Bubble: square-ish notices. */
#mamBubble[tone="notice"] { qproperty-radius: 3; }

/* Wallpaper behind any theme, and hover tinting back on in Bubble. */
MamChatWidget {
    qproperty-backgroundImage: url(/home/me/Pictures/chat-paper.png);
    qproperty-backgroundImageMode: tile;
    qproperty-hoverHighlight: true;
}
```

### How rules combine with Psi's colours

- Psi's colours for nicknames, times and text are defaults. A rule that sets
  `color` on a part wins, and keeps winning when Psi recomputes its colours
  (nickname colour, palette or font changes).
- Fonts in rules win over the chat font for that part. Psi still scales the
  parts you do not style.
- When a theme brings a style sheet of its own (see below), Qt prefers it to
  `options.ui.chat.css` for conflicting declarations, whatever the
  specificity: the theme's sheet is set closer to the parts.

### Limitations

- The row surface belongs to Psi: it is the log's background colour, tinted
  while the pointer is over a message and while messages are selected. Do not
  set `background` on `#mamMessageRow`; use the chat log properties above or
  the chat background colour option instead.
- Reaction chips (`#mamReactionChip`) and the hover action toolbar
  (`#mamMessageActions`) have style sheets of their own, which win over
  `options.ui.chat.css` for the declarations they make.
- A `qproperty-` value stays after its rule is removed: on existing bubbles
  until their rows are rebuilt (a theme switch, a reopened chat), and on the
  log until the chat is reopened.
- Layout (where the parts are, which ones are shown, their order) is not a
  style sheet matter. It is defined by the theme.

## 3. A theme plugin with a description

A plugin adds themes to the theme list by implementing
`NativeChatThemeProvider` (`plugins/include/nativechatthemeprovider.h`). Psi
copies the descriptions when the plugin is enabled: no plugin code runs while
the log is drawn, and the plugin can be unloaded at any time.

```cpp
#include "nativechatthemeprovider.h"
#include "plugininfoprovider.h"
#include "psiplugin.h"

class CompactThemes : public QObject, public PsiPlugin, public PluginInfoProvider, public NativeChatThemeProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.psi-plus.CompactThemes" FILE "psiplugin.json")
    Q_INTERFACES(PsiPlugin PluginInfoProvider NativeChatThemeProvider)
public:
    QString  name() const override { return QStringLiteral("Compact Themes"); }
    QWidget *options() override { return nullptr; }
    bool     enable() override { return true; }
    bool     disable() override { return true; }
    void     applyOptions() override { }
    void     restoreOptions() override { }
    QString  pluginInfo() override { return tr("Compact native chat themes."); }

    QList<NativeChatTheme> nativeChatThemes() const override
    {
        NativeChatTheme compact;
        compact.id         = QStringLiteral("org.example.compact"); // Globally unique.
        compact.name       = tr("Compact");
        compact.avatarSize = 24;  // 0 hides avatars; at most 96.
        compact.spacing    = 6;   // Between the avatar and the message; at most 32.
        compact.layoutSpec = QStringLiteral(
            R"({"direction":"horizontal","items":["avatar",{"direction":"vertical","items":[{"direction":"horizontal","items":["author","timestamp"]},"body"]}]})");
        compact.styleSheet = QStringLiteral(
            "QLabel#mamTimestamp { color: palette(mid); }"
            "QTextEdit#mamBody { padding-left: 2px; }");

        NativeChatTheme irc;
        irc.id         = QStringLiteral("org.example.irc");
        irc.name       = tr("IRC");
        irc.avatarSize = 0;
        irc.spacing    = 4;
        irc.timestampFormat = QStringLiteral("HH:mm:ss");
        irc.layoutSpec = QStringLiteral(
            R"({"direction":"horizontal","items":["timestamp","author","body"]})");

        NativeChatTheme paper = compact;
        paper.id                  = QStringLiteral("org.example.paper");
        paper.name                = tr("Paper");
        paper.hoverHighlight      = false;
        paper.background          = QColor(0xf4, 0xef, 0xe4);
        paper.backgroundImage     = QStringLiteral(":/compactthemes/paper.png"); // In the plugin's resources.
        paper.backgroundImageMode = NativeChatTheme::Tile;
        return { compact, irc, paper };
    }
};
```

The fields:

- `id`: a globally unique, stable identifier, saved in the user's options.
  Use a reverse domain name. Themes whose id is taken are ignored.
- `name`: shown in the theme list.
- `description`: optional explanation in the theme picker.
- `setRenderer(std::shared_ptr<const NativeChatMessageRenderer>)`: optional
  C++ message renderer owned by the theme. Without it Psi interprets
  `layoutSpec` as a generic part layout.
- `avatarSize`, `spacing`: in pixels.
- `styleSheet`: a Qt style sheet set on every message row of the theme, with
  the selectors from section 2. Prefer `palette(...)` colours so the theme
  follows light and dark palettes.
- `layoutSpec`: optional JSON that arranges the parts. With no C++ renderer
  and no valid JSON specification, a neutral avatar/header/body layout is used.
- `timestampFormat`: optional Qt date/time format. Empty follows the user's
  clock/relative setting; `HH:mm:ss` forces a clock with seconds;
  `{relative}` forces relative time, and `HH:mm '·' {relative}` combines both.
  A declarative JSON `layoutSpec` also accepts `"timestampFormat"` at its
  root; the theme field wins if both are supplied. Relative time needs a
  refresh only when its displayed value changes.
- `hoverHighlight`: whether the message under the pointer is tinted (default
  `true`; off for built-in Classic and Bubble). Hover actions still appear.
  Users can override this in the shared chat style sheet.
- `background`: the log's background colour; invalid (the default) uses the
  chat background colour option, which may itself follow the system palette.
  Choose it together with text colours that stay readable on it.
- `backgroundImage`, `backgroundImageMode`: an image file or Qt resource path
  painted behind the messages, tiled (`Tile`, the default) or scaled to cover
  the log (`Scale`). Psi loads it when the theme is applied, so an image in
  the plugin's resources stays shown after the plugin is unloaded.

Users can override the log properties and restyle the parts with
`options.ui.chat.css` (section 2).

The provider interface is `org.psi-im.NativeChatThemeProvider/2.0`.
The theme object itself owns its optional renderer; plugins do not implement
a second renderer-provider interface.

### `layoutSpec`

A tree of groups. A group is an object with `direction` (`"horizontal"` or
`"vertical"`) and `items` (1–12 entries). An item is a nested group or one of
the slot names `"avatar"`, `"author"`, `"timestamp"`, `"body"`,
`"dateSeparator"`, `"unreadSeparator"` and `"noticeTimestamp"`. Each slot
may appear once, `"body"` is required, and groups nest at most 8 levels deep.
Parts left out are not shown; leaving out `"avatar"` also stops Psi from
loading avatars. An invalid tree is ignored and the base layout is used.

```json
{
  "timestampFormat": "HH:mm:ss",
  "direction": "horizontal",
  "items": [
    "avatar",
    { "direction": "vertical",
      "items": [
        { "direction": "horizontal", "items": ["author", "timestamp"] },
        "body"
      ] }
  ]
}
```

That reproduces a Modern-like arrangement using JSON. A horizontal group inside a vertical one
becomes a `#mamHeaderGroup` widget.

### Building and installing

Theme plugins are ordinary Psi plugins: build them like the plugins in the
[psi-im/plugins](https://github.com/psi-im/plugins) repository, with
`plugins/include` on the include path, and install the library into a Psi
plugin directory. Enable the plugin in Options → Plugins; its themes appear
in Options → Appearance → Themes. Disabling the plugin removes them, and open
chats switch back to a built-in theme.

## 4. A theme plugin with a renderer

When JSON and stylesheets are insufficient (painted shapes, different
arrangements for incoming/outgoing messages, custom widgets), implement
`NativeChatMessageRenderer` and attach it directly to the theme with
`theme.setRenderer(std::make_shared<MyRenderer>())`. The theme's
`renderer()` method returns that object; Psi continues to own all semantic
parts and their behaviour (selection, actions, menus, media, relative times
and virtualization). See [native-chat-renderers.md](native-chat-renderers.md)
for the API and lifetime rules.

Rows built by a renderer can still be adjusted by `options.ui.chat.css` and
by the theme's `styleSheet`. Give the widgets you create object names and
dynamic properties (as Bubble does with `#mamBubble[tone="own"]`), and
declare `Q_PROPERTY`s for anything you paint yourself, so users can restyle
your theme the same way.
