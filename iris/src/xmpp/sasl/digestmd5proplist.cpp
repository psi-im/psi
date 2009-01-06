/*
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

#include "xmpp/sasl/digestmd5proplist.h"

namespace XMPP {

DIGESTMD5PropList::DIGESTMD5PropList() : QList<DIGESTMD5Prop>() 
{
}

void DIGESTMD5PropList::set(const QByteArray &var, const QByteArray &val) {
	DIGESTMD5Prop p;
	p.var = var;
	p.val = val;
	append(p);
}

QByteArray DIGESTMD5PropList::get(const QByteArray &var)
{
	for(ConstIterator it = begin(); it != end(); ++it) {
		if((*it).var == var)
			return (*it).val;
	}
	return QByteArray();
}

QByteArray DIGESTMD5PropList::toString() const
{
	QByteArray str;
	bool first = true;
	for(ConstIterator it = begin(); it != end(); ++it) {
		if(!first)
			str += ',';
		if ((*it).var == "realm" || (*it).var == "nonce" || (*it).var == "username" || (*it).var == "cnonce" || (*it).var == "digest-uri" || (*it).var == "authzid")
			str += (*it).var + "=\"" + (*it).val + '\"';
		else 
			str += (*it).var + "=" + (*it).val;
		first = false;
	}
	return str;
}

bool DIGESTMD5PropList::fromString(const QByteArray &str)
{
	DIGESTMD5PropList list;
	int at = 0;
	while(1) {
		while (at < str.length() && (str[at] == ',' || str[at] == ' ' || str[at] == '\t'))
				++at;
		int n = str.indexOf('=', at);
		if(n == -1)
			break;
		QByteArray var, val;
		var = str.mid(at, n-at);
		at = n + 1;
		if(str[at] == '\"') {
			++at;
			n = str.indexOf('\"', at);
			if(n == -1)
				break;
			val = str.mid(at, n-at);
			at = n + 1;
		}
		else {
			n = at;
			while (n < str.length() && str[n] != ',' && str[n] != ' ' && str[n] != '\t')
				++n;
			val = str.mid(at, n-at);
			at = n;
		}
		DIGESTMD5Prop prop;
		prop.var = var;
		if (var == "qop" || var == "cipher") {
			int a = 0;
			while (a < val.length()) {
				while (a < val.length() && (val[a] == ',' || val[a] == ' ' || val[a] == '\t'))
					++a;
				if (a == val.length())
					break;
				n = a+1;
				while (n < val.length() && val[n] != ',' && val[n] != ' ' && val[n] != '\t')
					++n;
				prop.val = val.mid(a, n-a);
				list.append(prop);
				a = n+1;
			}
		}
		else {
			prop.val = val;
			list.append(prop);
		}

		if(at >= str.size() - 1 || (str[at] != ',' && str[at] != ' ' && str[at] != '\t'))
			break;
	}

	// integrity check
	if(list.varCount("nonce") != 1)
		return false;
	if(list.varCount("algorithm") != 1)
		return false;
	*this = list;
	return true;
}

int DIGESTMD5PropList::varCount(const QByteArray &var)
{
	int n = 0;
	for(ConstIterator it = begin(); it != end(); ++it) {
		if((*it).var == var)
			++n;
	}
	return n;
}

}
