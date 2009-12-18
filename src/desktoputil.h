/*
 * desktoputil.h - url-opening routines
 * Copyright (C) 2007  Maciej Niedzielski, Michail Pishchagin
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

#ifndef DESKTOPUTIL_H
#define DESKTOPUTIL_H

class QString;
class QObject;
class QUrl;

namespace DesktopUtil
{
	bool openUrl(const QString& url);
	bool openUrl(const QUrl& url);

	void setUrlHandler(const QString& scheme, QObject* receiver, const char* method);
	void unsetUrlHandler(const QString& scheme);
}

#endif
