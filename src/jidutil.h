/*
 * jidutil.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef JIDUTIL
#define JIDUTIL

class QString;
namespace XMPP {
	class Jid;
}

class JIDUtil
{
public:
	static QString defaultDomain();
	static void setDefaultDomain(QString domain);

	static QString accountToString(const XMPP::Jid&, bool withResource);
	static XMPP::Jid accountFromString(const QString&);
	static QString toString(const XMPP::Jid&, bool withResource);
	static XMPP::Jid fromString(const QString&);
	static QString encode(const QString &jid);
	static QString decode(const QString &jid);
	static QString nickOrJid(const QString&, const QString&);

	static QString encode822(const QString&);
	static QString decode822(const QString&);
};

#endif
