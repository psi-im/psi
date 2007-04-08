/*
 * globalaccelman.h - manager for global hotkeys
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

#ifndef CS_GLOBALACCELMAN_H
#define CS_GLOBALACCELMAN_H

#include <qobject.h>

class QKeySequence;

class GlobalAccelManager : public QObject
{
	Q_OBJECT
public:
	GlobalAccelManager();
	~GlobalAccelManager();

	int setAccel(const QKeySequence &);
	void removeAccel(int);

signals:
	void activated(int);

private slots:
	void triggerActivated(int x) { activated(x); }

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
