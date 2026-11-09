/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NATIVECHATRENDERER_H
#define NATIVECHATRENDERER_H

#include <QColor>
#include <QDateTime>
#include <QString>
#include <QtPlugin>

class QWidget;

// Code-level appearance of messages in Psi's native chat log.
//
// Psi builds the semantic parts of every message itself: the selection check
// box, avatar, nickname, timestamp with its delivery/encryption/edit marks, and
// the body (text, reply preview, file cards and reactions). Psi also keeps all
// of their behaviour: selection, hover actions, context menus, link handling,
// relative-time refresh, avatar updates and virtualization of the log.
//
// A renderer decides where these parts go and what surrounds them. It may
// create any widgets of its own, including QWidget subclasses that paint
// themselves, and arrange the parts inside them with ordinary Qt layouts.
// Rows are measured with heightForWidth(), so use layouts that support it.
//
// See docs/native-chat-renderers.md for a walkthrough and the built-in
// theme providers/renderers in src/widgets/nativechat/themes/ for examples.

// What is being rendered. Values only; valid for the duration of the call.
struct NativeChatMessageInfo {
    enum Kind {
        Message, // An ordinary chat or group chat message.
        Notice,  // Status, presence, system and other informational events.
        Subject, // A group chat subject.
    };
    Kind      kind      = Message;
    bool      local     = false; // Sent by this account.
    bool      groupChat = false;
    bool      emote     = false; // A /me message. The body already starts with "*nick".
    bool      retracted = false;
    QString   nick;              // Display name of the sender; empty for notices.
    QDateTime time;
};

// How Psi builds and keeps styling its own parts. Psi applies these when the
// row is built and again whenever the chat font, palette or nickname colours
// change, without rebuilding the row. Renderers may restyle the parts further
// in arrange(), but Psi resets the properties listed here on such changes; use
// NativeChatMessageRenderer::restyle() to reapply anything else.
struct NativeChatPartOptions {
    int     avatarSize = 0;      // Pixels; 0 builds no avatar.
    QString timeFormat;          // Per-row output format, set by Psi's global
                                 // timestamp preference (clock or relative).
                                 // Theme renderers must not override that preference.
    QString authorSuffix;        // Appended to the nickname, e.g. ":".
    bool    boldAuthor     = true;
    qreal   timePointDelta = 0;  // Timestamp size relative to the chat font, in points.
    int     timeAlpha      = 255; // Opacity of the timestamp text colour.
    bool    timeInNickColor = false; // Paint the timestamp in the nickname colour.

    // Where the time and the nickname go. As parts of their own (Author,
    // Timestamp), or at the start of the message text, in one document with
    // it: wrapped lines then start at the left edge, as in Psi's classic text
    // log, and the message icon (direction, delivery, encryption) goes before
    // the time. Psi updates relative time inside that text document in place.
    enum Header { HeaderParts, HeaderInText };
    Header  header = HeaderParts;
    QString authorPrefix; // Put before the nickname, e.g. "<".
    // In a conversation (not a group chat), paint the nickname in the colour
    // of sent or received messages from Psi's options, as the legacy text
    // log did. With the header in the text, the time takes it too.
    bool directionColors = false;
};

// The row being built. Implemented by Psi; valid only during arrange().
class NativeChatMessageCanvas {
public:
    enum Part {
        Selection, // Check box shown while messages are selected.
        Avatar,    // Present when NativeChatPartOptions::avatarSize > 0 and kind == Message.
        Author,    // QLabel with the nickname. Messages with HeaderParts only.
        Timestamp, // The time with its delivery/encryption/edit marks. Messages with HeaderParts only.
        Body,      // Text with reply preview, file cards and reactions. Always present.
        UnreadSeparator, // Optional boundary frame; Psi places it below the row if not taken.
        DateSeparator, // Optional local calendar date label before the first row of a day.
    };

    // The row widget. Psi owns it, paints its hover/selection surface and
    // applies the theme's style sheet. Give it a layout; its children may be
    // any widgets of the renderer's choice.
    virtual QWidget *row() const = 0;

    // Hands over a part to be placed in the renderer's layout, or nullptr if
    // this message has no such part. Parts that are not taken are hidden.
    // UnreadSeparator is available only on the boundary row. Taking it lets
    // the theme position and style the frame; otherwise Psi places it below
    // the renderer's content as the default layout fallback.
    // DateSeparator is a QLabel, supplied only for the first message of a
    // local calendar day. Taking it lets the theme wrap/style/position it
    // (e.g. a centred date bubble). Otherwise Psi adds a divider above
    // the row. Neither optional part changes the plugin interface vtable.
    virtual QWidget *take(Part part) = 0;

    // The widget whose top-right corner anchors the hover actions toolbar.
    // Defaults to the row.
    virtual void setActionAnchor(QWidget *anchor) = 0;

    // Body text colour, e.g. for contrast on a coloured bubble. An invalid
    // colour (the default) uses the chat palette. Notices keep Psi's
    // informational colour unless set explicitly.
    virtual void setBodyTextColor(const QColor &color) = 0;

protected:
    ~NativeChatMessageCanvas() = default;
};

class NativeChatMessageRenderer {
public:
    virtual ~NativeChatMessageRenderer() = default;

    // Called before the parts are built.
    virtual NativeChatPartOptions partOptions(const NativeChatMessageInfo &info) const = 0;

    // Lays out the parts. Called once per row; rows are built again when the
    // message, its state or the theme changes.
    virtual void arrange(NativeChatMessageCanvas &canvas, const NativeChatMessageInfo &info) const = 0;

    // Called after Psi has restyled the parts of an existing row in place,
    // because the chat font, palette or nickname colours changed. Widgets that
    // read palette() and font() while painting need no action here.
    virtual void restyle(QWidget *row, const NativeChatMessageInfo &info) const
    {
        Q_UNUSED(row)
        Q_UNUSED(info)
    }
};

// A theme supplies an optional renderer using NativeChatTheme::setRenderer().
// Themes without a C++ renderer are interpreted from layoutSpec.
#endif
