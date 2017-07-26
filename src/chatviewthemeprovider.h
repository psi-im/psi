/*
 * chatviewthemeprovider.h - adapter for set of chatview themes
 * Copyright (C) 2010-2017 Sergey Ilinykh
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


#ifndef CHATVIEWTHEMEPROVIDER_H
#define CHATVIEWTHEMEPROVIDER_H

#include "psithemeprovider.h"

class QWebEngineUrlRequestInterceptor;
class ChatViewTheme;
class ThemeServer;
class PsiCon;

class ChatViewThemeProvider : public PsiThemeProvider
{
	Q_OBJECT

public:
	ChatViewThemeProvider(PsiCon *);

	const char* type() const { return "chatview"; }
	const QStringList themeIds() const;
	Theme theme(const QString &id);

	bool loadCurrent();
	Theme current() const; // currently loaded theme

	void setCurrentTheme(const QString &);
	virtual int screenshotWidth() const { return 512; } // hack

#ifdef WEBENGINE
	ThemeServer *themeServer();
	QWebEngineUrlRequestInterceptor *requestInterceptor();
#endif

	QString optionsName() const { return tr("Chat Message Style"); }
	QString optionsDescription() const { return tr("Configure your chat theme here"); }

protected:
	virtual const char* optionString() const { return "options.ui.chat.theme"; }

signals:
	void themeChanged();

private:
	PsiCon *_psi;
	Theme curTheme; // FIXME it does not make sense to keep it as pointer
};

class GroupChatViewThemeProvider : public ChatViewThemeProvider
{
	Q_OBJECT

public:
	GroupChatViewThemeProvider(PsiCon *psi) :
		ChatViewThemeProvider(psi) {}

	const char* type() const { return "groupchatview"; }
	QString optionsName() const { return tr("Groupchat Message Style"); }
	QString optionsDescription() const { return tr("Configure your groupchat theme here"); }

protected:
	const char* optionString() const { return "options.ui.muc.theme"; }
};

#endif // CHATVIEWTHEMEPROVIDER_H
