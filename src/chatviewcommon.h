/*
 * chatviewcommon.h - shared part of any chatview
 * Copyright (C) 2010  Sergey Ilinykh
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

#ifndef CHATVIEWBASE_H
#define CHATVIEWBASE_H

#include <QDateTime>
#include <QMap>
#include <QStringList>

class QWidget;

class ChatViewCommon
{
public:
	enum UserType {
		LocalParty,
		RemoteParty,
		Participant
	};


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
	QList<QColor> &generatePalette();
	bool compatibleColors(const QColor &, const QColor &);
	int _nickNumber;
	QMap<QString,int> _nicks;
};

#endif
