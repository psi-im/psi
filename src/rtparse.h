/****************************************************************************
** rtparse.h - class for manipulating richtext
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
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
**
****************************************************************************/

#ifndef RTPARSE_H
#define RTPARSE_H

#include <QString>

class RTParse
{
public:
	RTParse(const QString &);

	const QString &output() const;

	QString next();
	bool atEnd() const;
	void putPlain(const QString &);
	void putRich(const QString &);

private:
	QString in, out;
	int v_at;
	bool v_atEnd;
};

#endif
