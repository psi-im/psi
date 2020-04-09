/*
 * chatviewtheme.h - theme for webkit based chatview
 * Copyright (C) 2010-2017  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef CHATVIEWTHEME_H
#define CHATVIEWTHEME_H

#include "theme.h"
#include "webview.h"

#include <QPointer>
#include <functional>

class ThemeServer;

class ChatViewThemeSession : public QObject {
    Q_OBJECT

    friend class ChatViewThemePrivate;

    Theme   theme;
    QString sessId; // unique id of session

public:
    ChatViewThemeSession(QObject *parent = nullptr);
    virtual ~ChatViewThemeSession();

    inline const QString &sessionId() const { return sessId; }
    virtual WebView *     webView() = 0;
    // returns: data, content-type
    virtual bool getContents(const QUrl &                                                              url,
                             std::function<void(bool success, const QByteArray &, const QByteArray &)> callback)
        = 0;
    QString propsAsJsonString();

    void init(const Theme &theme);
    bool isTransparentBackground() const;
private slots:
#ifndef WEBENGINE
    void embedJsObject();
#endif
};

#endif // CHATVIEWTHEME_H
