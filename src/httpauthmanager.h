/*
 * httpauthmanager.h - Classes managing incoming HTTP-Auth requests
 * Copyright (C) 2006  Maciej Niedzielski
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

#ifndef HTTPAUTHMANAGER_H
#define HTTPAUTHMANAGER_H

#include <QObject>

class HttpAuthListener;
namespace XMPP {
	class Task;
}
class PsiHttpAuthRequest;

class HttpAuthManager : public QObject
{
	Q_OBJECT

public:
	HttpAuthManager(XMPP::Task *);
	~HttpAuthManager();

public slots:
	void confirm(const PsiHttpAuthRequest &req);
	void deny(const PsiHttpAuthRequest &req);

signals:
	void confirmationRequest(const PsiHttpAuthRequest &);

private:
	HttpAuthListener *listener_;
};

#endif
