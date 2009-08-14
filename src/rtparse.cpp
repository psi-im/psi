/****************************************************************************
** rtparse.cpp - class for manipulating richtext
** Copyright (C) 2001, 2002  Justin Karneges
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,USA.
**
****************************************************************************/

#include "rtparse.h"
#include "textutil.h"

#include <QTextDocument> // for Qt::escape()

RTParse::RTParse(const QString &_in)
{
	in = _in;
	v_atEnd = in.length() == 0;
	v_at = 0;
	//printf("rtparse:\n");
}

const QString &RTParse::output() const
{
	//printf("final: [%s]\n", out.latin1());
	return out;
}

QString RTParse::next()
{
	if(v_atEnd)
		return "";

	// if we're at a tag, append it to the output
	if(in.at(v_at) == '<') {
		QString s;
		int n = in.indexOf('>', v_at);
		if(n == -1) {
			s = in.mid(v_at);
		}
		else {
			++n;
			s = in.mid(v_at, n-v_at);
		}
		v_at += s.length();
		out += s;
	}

	// now find the next tag, and grab the text in between
	QString s;
	int x = in.indexOf('<', v_at);
	if(x == -1) {
		s = in.mid(v_at);
		v_atEnd = true;
	}
	else {
		s = in.mid(v_at, x-v_at);
	}
	v_at += s.length();
	//printf("chunk = '%s'\n", s.latin1());
	s = TextUtil::resolveEntities(s);
	//printf("resolved = '%s'\n", s.latin1());
	return s;
}

bool RTParse::atEnd() const
{
	return v_atEnd;
}

void RTParse::putPlain(const QString &s)
{
	//printf("got this: [%s]\n", s.latin1());
	out += Qt::escape(s);
	//printf("changed to this: [%s]\n", expandEntities(s).latin1());
}

void RTParse::putRich(const QString &s)
{
	out += s;
	//printf("+ '%s'\n", s.latin1());
}

