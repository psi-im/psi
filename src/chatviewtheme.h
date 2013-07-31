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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CHATVIEWTHEME_H
#define CHATVIEWTHEME_H

#include <QPointer>

#include "theme.h"
#include "webview.h"

class ChatViewThemeJS;

class ChatViewTheme : public Theme
{
	friend class ChatViewThemeJS;
public:
	ChatViewTheme(const QString &id);
	~ChatViewTheme();

	QByteArray screenshot();

	bool load( const QString &file, const QStringList &helperScripts,
			   const QString &adapterPath );

	QObject * jsHelper();
	QStringList scripts();
	QString html(QObject *session = 0);
	QString jsNamespace();
	inline QUrl baseUrl() const { return QUrl("theme://messages/" + id() + "/"); }

private:
	class Private;
	Private *d;
};

#endif
