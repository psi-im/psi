/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../plugins/include/nativechatthemeprovider.h"
#include <QMap>
#include <QObject>

class NativeChatThemes : public QObject {
    Q_OBJECT
public:
    static NativeChatThemes *instance();
    QList<NativeChatTheme>   themes() const;
    NativeChatTheme          theme(const QString &id) const;
    void                     add(QObject *owner, const QList<NativeChatTheme> &themes);
    void                     remove(QObject *owner);
signals:
    void changed();

private:
    explicit NativeChatThemes(QObject *parent);
    QMap<QObject *, QList<NativeChatTheme>> plugins_;
};
