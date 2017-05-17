/*
 * chatviewtheme.h - theme for webkit based chatview
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CHATVIEWTHEME_H
#define CHATVIEWTHEME_H

#include <QPointer>
#include <functional>

#include "webview.h"
#include "theme.h"

class SessionRequestHandler;
class ThemeServer;



class ChatViewThemeSession {
	friend class ChatViewThemePrivate;
#ifndef WEBENGINE
	friend class SessionRequestHandler;
#endif

	QString sessId; // unique id of session

#ifdef WEBENGINE
	ThemeServer *server = 0;
#endif

public:
	virtual ~ChatViewThemeSession();

	inline const QString &sessionId() const { return sessId; }
	virtual WebView* webView() = 0;
	virtual QObject* jsBridge() = 0;
	virtual Theme theme() const = 0;
	// returns: data, content-type
	virtual QPair<QByteArray,QByteArray> getContents(const QUrl &url) = 0;
	virtual QString propsAsJsonString() const = 0;

	static void init(QSharedPointer<ChatViewThemeSession> sess);
	bool isTransparentBackground() const;
};

#endif
