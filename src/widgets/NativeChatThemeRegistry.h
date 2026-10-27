/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../plugins/include/nativechatrenderer.h"
#include "../../plugins/include/nativechatthemeprovider.h"
#include <QHash>
#include <QMap>
#include <QObject>

class NativeChatThemes : public QObject {
    Q_OBJECT
public:
    static NativeChatThemes *instance();
    QList<NativeChatTheme>   themes() const;
    NativeChatTheme          theme(const QString &id) const;
    // nullptr: lay the theme out from its description.
    const NativeChatMessageRenderer *renderer(const QString &id) const;
    // Registers a plugin's themes. With a provider, also the renderers it
    // offers for them; see hasRenderers().
    void add(QObject *owner, const QList<NativeChatTheme> &themes,
             const NativeChatRendererProvider *renderers = nullptr);
    void remove(QObject *owner);
    // Whether the owner's registered themes include code renderers. Rows they
    // built may outlive remove(), so the owner's code must stay loaded.
    bool hasRenderers(QObject *owner) const;
signals:
    void changed();

private:
    explicit NativeChatThemes(QObject *parent);
    struct PluginThemes {
        QList<NativeChatTheme>                          themes;
        QHash<QString, const NativeChatMessageRenderer *> renderers;
    };
    QMap<QObject *, PluginThemes> plugins_;
};
