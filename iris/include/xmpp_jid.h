/*
 * xmpp_jid.h
 * Copyright (C) 2003  Justin Karneges
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

#ifndef XMPP_JID_H
#define XMPP_JID_H

#include <QString>

namespace XMPP 
{
	class Jid
	{
	public:
		Jid();
		~Jid();

		Jid(const QString &s);
		Jid(const char *s);
		Jid & operator=(const QString &s);
		Jid & operator=(const char *s);

		void set(const QString &s);
		void set(const QString &domain, const QString &node, const QString &resource="");

		void setDomain(const QString &s);
		void setNode(const QString &s);
		void setResource(const QString &s);

		bool isNull() const { return null; }
		const QString & domain() const { return d; }
		const QString & node() const { return n; }
		const QString & resource() const { return r; }
		const QString & bare() const { return b; }
		const QString & full() const { return f; }

		Jid withNode(const QString &s) const;
		Jid withResource(const QString &s) const;

		bool isValid() const;
		bool isEmpty() const;
		bool compare(const Jid &a, bool compareRes=true) const;

		static bool validDomain(const QString &s, QString *norm=0);
		static bool validNode(const QString &s, QString *norm=0);
		static bool validResource(const QString &s, QString *norm=0);

		// TODO: kill these later
		const QString & host() const { return d; }
		const QString & user() const { return n; }
		const QString & userHost() const { return b; }

	private:
		void reset();
		void update();

		QString f, b, d, n, r;
		bool valid, null;
	};
}

#endif
