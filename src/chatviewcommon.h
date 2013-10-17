/*
 * chatviewcommon.h - shared part of any chatview
 * Copyright (C) 2010 Rion
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

#ifndef CHATVIEWBASE_H
#define CHATVIEWBASE_H

#include <QDateTime>
#include <QMap>
#include <QStringList>

class QWidget;

class ChatViewCommon
{
public:
	ChatViewCommon() : _nickNumber(0) { }
	void setLooks(QWidget *);
	inline const QDateTime& lastMsgTime() const { return _lastMsgTime; }
	bool updateLastMsgTime(QDateTime t);
	QString getMucNickColor(const QString &, bool,
							QStringList validList = QStringList());
	QList<QColor> getPalette();

protected:
	QDateTime _lastMsgTime;

private:
	void generatePalette();
	bool compatibleColors(const QColor &, const QColor &);
	int _nickNumber;
	QMap<QString,int> _nicks;
	QList<QColor> _palette;
};

#endif
