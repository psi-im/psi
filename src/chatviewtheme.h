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

#include "theme.h"
#include "webview.h"

class ChatViewThemeJSUtil;
class ChatViewJSLoader;
class ChatViewThemeProvider;
class ChatViewThemePrivate;
class ChatViewThemeSession;



class ChatViewTheme : public Theme
{
	friend class ChatViewThemeJSUtil;
public:

	ChatViewTheme();
	ChatViewTheme(ChatViewThemeProvider *provider);
	ChatViewTheme(const ChatViewTheme &other);
	ChatViewTheme &operator=(const ChatViewTheme &other);
	~ChatViewTheme();

	QByteArray screenshot();

	bool load(const QString &id, std::function<void(bool)> loadCallback);

	bool isMuc() const;
	QString jsNamespace();
	inline QUrl baseUrl() const { return QUrl("theme://messages/" + id() + "/"); }

	void putToCache(const QString &key, const QVariant &data);
	void setTransparentBackground(bool enabled = true);
	bool isTransparentBackground() const;
	bool applyToWebView(QSharedPointer<ChatViewThemeSession> session);

private:
	friend class ChatViewJSLoader;
	friend class ChatViewThemePrivate;

	// theme is destroyed only when all chats using it are closed (TODO: ensure)
	QExplicitlySharedDataPointer<ChatViewThemePrivate> cvtd;
};

class ThemeServer;
class ChatViewThemeSession {
	friend class ChatViewTheme;

	QString sessId; // unique id of session
	ChatViewTheme theme;
	ThemeServer *server = 0;

public:
	virtual ~ChatViewThemeSession();

	inline const QString &sessionId() const { return sessId; }
	virtual WebView* webView() = 0;
	virtual QObject* jsBridge() = 0;
	// returns: data, content-type
	virtual QPair<QByteArray,QByteArray> getContents(const QUrl &url) = 0;
};

#endif
