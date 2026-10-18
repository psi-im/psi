/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NATIVECHATTHEMEPROVIDER_H
#define NATIVECHATTHEMEPROVIDER_H

#include <QList>
#include <QString>
#include <QtPlugin>

// Value-only API: Psi copies these descriptions on plugin enable, so no
// plugin code, widgets or callbacks survive disabling/unloading the plugin.
struct NativeChatTheme {
    enum Layout { Modern, Classic };
    QString id; // Plugin themes must use a globally unique ID (e.g. org.example.theme).
    QString name;
    Layout  layout     = Modern;
    int     avatarSize = 40;
    int     spacing    = 10;
    QString styleSheet; // Scoped to each message row. Prefer palette() colours.
    // Optional JSON layout tree: direction = horizontal/vertical; items are nested
    // groups or the author, timestamp, avatar, body slots. No plugin-owned widgets.
    QString layoutSpec;
};

class NativeChatThemeProvider {
public:
    virtual ~NativeChatThemeProvider()                      = default;
    virtual QList<NativeChatTheme> nativeChatThemes() const = 0;
};
Q_DECLARE_INTERFACE(NativeChatThemeProvider, "org.psi-im.NativeChatThemeProvider/1.0")
#endif
