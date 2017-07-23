/*
 * chatviewthemeprovider_priv.h
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CHATVIEWTHEMEPROVIDER_PRIV_H
#define CHATVIEWTHEMEPROVIDER_PRIV_H

#ifdef WEBENGINE
#include <QWebEngineUrlRequestInterceptor>
#else
#include <QObject> // at least
#endif

class PsiCon;
class ThemeServer;

#ifdef WEBENGINE
class ChatViewUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
	Q_OBJECT
	Q_DISABLE_COPY(ChatViewUrlRequestInterceptor)
public:
	explicit ChatViewUrlRequestInterceptor(QObject *parent = Q_NULLPTR);
	void interceptRequest(QWebEngineUrlRequestInfo &info);
};
#endif

class ChatViewCon : public QObject
{
	Q_OBJECT

	PsiCon *pc;

	ChatViewCon(PsiCon *pc);
public:
	~ChatViewCon();
#ifdef WEBENGINE
	ThemeServer *themeServer;
	ChatViewUrlRequestInterceptor *requestInterceptor;
#endif
	static ChatViewCon* instance();
	static void init(PsiCon *pc);
	static bool isReady();
};

#endif // CHATVIEWTHEMEPROVIDER_PRIV_H
