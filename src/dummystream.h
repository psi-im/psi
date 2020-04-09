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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef DUMMYSTREAM_H
#define DUMMYSTREAM_H

#include "xmpp_stanza.h"
#include "xmpp_stream.h"

class DummyStream : public XMPP::Stream {
public:
    QDomDocument &doc() const { return v_doc; }
    QString       baseNS() const { return "jabber:client"; }
    bool          old() const { return false; }

    void         close() {}
    bool         stanzaAvailable() const { return false; }
    XMPP::Stanza read() { return XMPP::Stanza(); }
    void         write(const XMPP::Stanza &) {}

    int                     errorCondition() const { return 0; }
    QString                 errorText() const { return QString(); }
    QHash<QString, QString> errorLangText() const { return QHash<QString, QString>(); }
    QDomElement             errorAppSpec() const { return v_doc.documentElement(); }

private:
    static QDomDocument v_doc;
};

#endif // DUMMYSTREAM_H
