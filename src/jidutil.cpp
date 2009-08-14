/*
 * jidutil.h
 * Copyright (C) 2006  Remko Troncon, Justin Karneges
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

#include "xmpp_jid.h"
#include "jidutil.h"
#include "psioptions.h"

using namespace XMPP;

QString JIDUtil::defaultDomain()
{
	return PsiOptions::instance()->getOption("options.account.domain").toString();
}

void JIDUtil::setDefaultDomain(QString domain)
{
	PsiOptions::instance()->setOption("options.account.domain", domain);
}

QString JIDUtil::encode(const QString &jid)
{
	QString jid2;

	for(int n = 0; n < jid.length(); ++n) {
		if(jid.at(n) == '@') {
			jid2.append("_at_");
		}
		else if(jid.at(n) == '.') {
			jid2.append('.');
		}
		else if(!jid.at(n).isLetterOrNumber()) {
			// hex encode
			QString hex;
			hex.sprintf("%%%02X", jid.at(n).toLatin1());
			jid2.append(hex);
		}
		else {
			jid2.append(jid.at(n));
		}
	}

	return jid2;
}

QString JIDUtil::decode(const QString &jid)
{
	QString jid2;
	int n;

	for(n = 0; n < (int)jid.length(); ++n) {
		if(jid.at(n) == '%' && (jid.length() - n - 1) >= 2) {
			QString str = jid.mid(n+1,2);
			bool ok;
			char c = str.toInt(&ok, 16);
			if(!ok)
				continue;

			QChar a(c);
			jid2.append(a);
			n += 2;
		}
		else {
			jid2.append(jid.at(n));
		}
	}

	// search for the _at_ backwards, just in case
	for(n = (int)jid2.length(); n >= 3; --n) {
		if(jid2.mid(n, 4) == "_at_") {
			jid2.replace(n, 4, "@");
			break;
		}
	}

	return jid2;
}

QString JIDUtil::nickOrJid(const QString &nick, const QString &jid)
{
	if(nick.isEmpty())
		return jid;
	else
		return nick;
}

QString JIDUtil::accountToString(const Jid& j, bool withResource)
{
	QString s = j.node();
	if (!defaultDomain().isEmpty()) {
		return (withResource && !j.resource().isEmpty() ? j.node() + "/" + j.resource() : j.node());
	}
	else {
		return (withResource ? j.full() : j.bare());
	}
}

Jid JIDUtil::accountFromString(const QString& s)
{
	if (!defaultDomain().isEmpty()) {
		return Jid(s, defaultDomain(), "");
	}
	else {
		return Jid(s);
	}
}

QString JIDUtil::toString(const Jid& j, bool withResource)
{
	return (withResource ? j.full() : j.bare());
}

Jid JIDUtil::fromString(const QString& s)
{
	return Jid(s);
}

QString JIDUtil::encode822(const QString &s)
{
	QString out;
	for(int n = 0; n < (int)s.length(); ++n) {
		if(s[n] == '\\' || s[n] == '<' || s[n] == '>') {
			QString hex;
			hex.sprintf("\\x%02X", (unsigned char )s[n].toLatin1());
			out.append(hex);
		}
		else
			out += s[n];
	}
	return out;
}

QString JIDUtil::decode822(const QString &s)
{
	QString out;
	for(int n = 0; n < (int)s.length(); ++n) {
		if(s[n] == '\\' && n + 3 < (int)s.length()) {
			int x = n + 1;
			n += 3;
			if(s[x] != 'x')
				continue;
			ushort val = 0;
			val += QString(s[x+1]).toInt(NULL,16);
			val += QString(s[x+2]).toInt(NULL,16);
			QChar c(val);
			out += c;
		}
		else
			out += s[n];
	}
	return out;
}

