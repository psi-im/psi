/*
 * Copyright (C) 2006  Maciek Niedzielski
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef XMPP_AUTHREQUEST_H
#define XMPP_AUTHREQUEST_H

#include <QString>

class QDomElement;
class QDomDocument;

namespace XMPP
{
	class HttpAuthRequest
	{
	public:
		HttpAuthRequest(const QString &m, const QString &u, const QString &i);
		HttpAuthRequest(const QString &m = "", const QString &u = "");
		HttpAuthRequest(const QDomElement &);

		bool isEmpty() const;

		void setMethod(const QString&);
		void setUrl(const QString&);
		void setId(const QString&);
		QString method() const;
		QString url() const;
		QString id() const;
		bool hasId() const;

		QDomElement toXml(QDomDocument &) const;
		bool fromXml(const QDomElement &);

		static Stanza::Error denyError;
	private:
		QString method_, url_, id_;
		bool hasId_;
	};
}

#endif
