/*
 * jid.h
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

#ifdef IRIS_XMPP_JID_DEPRECATED
#define IRIS_XMPP_JID_DECL_DEPRECATED Q_DECL_DEPRECATED
#else
#define IRIS_XMPP_JID_DECL_DEPRECATED
#endif

namespace XMPP 
{
	class Jid
	{
	public:
		Jid();
		~Jid();

		Jid(const QString &s);
		Jid(const QString &node, const QString& domain, const QString& resource = "");
		Jid(const char *s);
		Jid & operator=(const QString &s);
		Jid & operator=(const char *s);

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

#ifdef IRIS_XMPP_JID_DEPRECATED
		IRIS_XMPP_JID_DECL_DEPRECATED const QString & host() const { return d; }
		IRIS_XMPP_JID_DECL_DEPRECATED const QString & user() const { return n; }
		IRIS_XMPP_JID_DECL_DEPRECATED const QString & userHost() const { return b; }
#endif

	private:
#ifdef IRIS_XMPP_JID_DEPRECATED
	public:
#endif
		IRIS_XMPP_JID_DECL_DEPRECATED void set(const QString &s);
		IRIS_XMPP_JID_DECL_DEPRECATED void set(const QString &domain, const QString &node, const QString &resource="");

		IRIS_XMPP_JID_DECL_DEPRECATED void setDomain(const QString &s);
		IRIS_XMPP_JID_DECL_DEPRECATED void setNode(const QString &s);
		IRIS_XMPP_JID_DECL_DEPRECATED void setResource(const QString &s);

	private:
		void reset();
		void update();

		QString f, b, d, n, r;
		bool valid, null;
	};
}

#endif
