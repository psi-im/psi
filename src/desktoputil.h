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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef DESKTOPUTIL_H
#define DESKTOPUTIL_H

class QObject;
class QString;
class QUrl;

namespace DesktopUtil {
    bool openUrl(const QString& url);
    bool openUrl(const QUrl& url);

    void setUrlHandler(const QString& scheme, QObject* receiver, const char* method);
    void unsetUrlHandler(const QString& scheme);
}; // namespace DesktopUtil

#endif
