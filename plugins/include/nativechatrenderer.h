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
// renderers in src/widgets/NativeChatRenderers.cpp for complete examples.

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
    QString timeFormat;          // QDateTime format, e.g. "HH:mm" or "[HH:mm:ss]".
                                 // Empty: relative time ("5 minutes ago"), refreshed by Psi.
    QString authorSuffix;        // Appended to the nickname, e.g. ":".
    bool    boldAuthor     = true;
    qreal   timePointDelta = 0;  // Timestamp size relative to the chat font, in points.
    int     timeAlpha      = 255; // Opacity of the timestamp text colour.
    bool    timeInNickColor = false; // Paint the timestamp in the nickname colour.
};

// The row being built. Implemented by Psi; valid only during arrange().
class NativeChatMessageCanvas {
public:
    enum Part {
        Selection, // Check box shown while messages are selected.
        Avatar,    // Present when NativeChatPartOptions::avatarSize > 0 and kind == Message.
        Author,    // QLabel with the nickname. Messages only.
        Timestamp, // The time with its delivery/encryption/edit marks. Messages only.
        Body,      // Text with reply preview, file cards and reactions. Always present.
    };

    // The row widget. Psi owns it, paints its hover/selection surface and
    // applies the theme's style sheet. Give it a layout; its children may be
    // any widgets of the renderer's choice.
    virtual QWidget *row() const = 0;

    // Hands over a part to be placed in the renderer's layout, or nullptr if
    // this message has no such part. Parts that are not taken are hidden.
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

// Implemented by a plugin's root object next to NativeChatThemeProvider.
//
// Psi asks for a renderer for each theme the plugin declared. The renderer
// and the code of the widgets it creates must stay valid until the plugin
// object is destroyed: while such a plugin is disabled at runtime its themes
// are removed and existing rows are rebuilt with a built-in theme, but its
// library stays loaded until Psi exits, so rows built earlier can die safely.
class NativeChatRendererProvider {
public:
    virtual ~NativeChatRendererProvider() = default;
    // nullptr: Psi lays the theme out from its NativeChatTheme description.
    virtual const NativeChatMessageRenderer *nativeChatRenderer(const QString &themeId) const = 0;
};
Q_DECLARE_INTERFACE(NativeChatRendererProvider, "org.psi-im.NativeChatRendererProvider/1.0")

#endif
