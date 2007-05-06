/*
 * pgptransaction.h 
 * Copyright (C) 2001-2005  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#ifndef PGPTRANSACTION_H
#define PGPTRANSACTION_H

#include <QDomElement>
#include <QtCrypto>

#include "xmpp_jid.h"
#include "xmpp_message.h"


class PGPTransaction : public QCA::SecureMessage
{
	Q_OBJECT

public:
	PGPTransaction(QCA::SecureMessageSystem*);
	~PGPTransaction();

	int id() const;

	const XMPP::Message& message() const;
	void setMessage(const XMPP::Message&);

	const QDomElement & xml() const;
	void setXml(const QDomElement &);

	XMPP::Jid jid() const;
	void setJid(const XMPP::Jid &);

private:
	QCA::SecureMessageSystem* system_;
	XMPP::Message message_;
	QDomElement xml_;
	XMPP::Jid jid_;
	int id_;
	static int idCounter_;
};

#endif
