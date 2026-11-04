/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../plugins/include/nativechatthemeprovider.h"
#include <QList>
#include <QObject>

class NativeChatThemes : public QObject {
    Q_OBJECT
public:
    static NativeChatThemes *instance();
    QList<NativeChatTheme> themes() const;
    NativeChatTheme theme(const QString &id) const;
    // Register themes from either a built-in or external provider.
    void add(QObject *owner, const QList<NativeChatTheme> &themes);
    void remove(QObject *owner);
    // Custom C++ renderers can leave plugin widgets awaiting deferred deletion.
    bool hasCustomRenderers(QObject *owner) const;
signals:
    void changed();
private:
    explicit NativeChatThemes(QObject *parent);
    struct Registration {
        QObject *owner = nullptr;
        QList<NativeChatTheme> themes;
    };
    // Preserve order: the first registered theme is the default.
    QList<Registration> registrations_;
};
