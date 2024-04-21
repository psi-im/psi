/*
 * filesharingproxy.h - proxy network access reply for shared files
 * Copyright (C) 2024  Sergey Ilinykh
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

#ifndef FILESHARINGPROXY_H
#define FILESHARINGPROXY_H

class QString;
class QNetworkRequest;
class QNetworkReply;

class PsiAccount;

namespace qhttp { namespace server {
    class QHttpRequest;
    class QHttpResponse;
}}

namespace FileSharingProxy {
#ifdef HAVE_WEBSERVER
void proxify(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *request,
             qhttp::server::QHttpResponse *response);
#endif
QNetworkReply *proxify(PsiAccount *acc, const QString &sourceIdHex, const QNetworkRequest &req);

};

#endif // FILESHARINGPROXY_H
