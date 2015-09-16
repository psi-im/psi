/*
 * dummystream.h - dummy Stream class for saving stanzas to strings
 * Copyright (C) 2001-2010  Justin Karneges
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef DUMMYSTREAM_H
#define DUMMYSTREAM_H

#include "xmpp_stream.h"
#include "xmpp_stanza.h"

class DummyStream : public XMPP::Stream
{
public:
	QDomDocument & doc() const { return v_doc; }
	QString baseNS() const { return "jabber:client"; }
	bool old() const { return false; }

	void close() { }
	bool stanzaAvailable() const { return false; }
	XMPP::Stanza read() { return XMPP::Stanza(); }
	void write(const XMPP::Stanza &, bool = false) { }

	int errorCondition() const { return 0; }
	QString errorText() const { return QString::null; }
	QDomElement errorAppSpec() const { return v_doc.documentElement(); }

private:
	static QDomDocument v_doc;
};


#endif
