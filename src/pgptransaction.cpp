/*
 * pgptransaction.cpp
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

#include <QtCrypto>

#include "pgptransaction.h"

using namespace XMPP;

PGPTransaction::PGPTransaction(QCA::SecureMessageSystem* system) : QCA::SecureMessage(system), system_(system)
{
	id_ = idCounter_++;
}

PGPTransaction::~PGPTransaction()
{
	delete system_;
}

int PGPTransaction::id() const
{
	return id_;
}

void PGPTransaction::setMessage(const XMPP::Message &m)
{
	message_ = m;
}

const XMPP::Message & PGPTransaction::message() const
{
	return message_;
}

const QDomElement & PGPTransaction::xml() const
{
	return xml_;
}

void PGPTransaction::setXml(const QDomElement &xml)
{
	xml_ = xml;
}

Jid PGPTransaction::jid() const
{
	return jid_;
}

void PGPTransaction::setJid(const Jid &j)
{
	jid_ = j;
}

int PGPTransaction::idCounter_ = 0;
