/*
 * systemwatch.h - Detect changes in the system state.
 * Copyright (C) 2005  Remko Troncon
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

#ifndef SYSTEMWATCH_H
#define SYSTEMWATCH_H

#include <qobject.h>

class SystemWatch : public QObject
{
	Q_OBJECT

public:
	static SystemWatch* instance();

signals:
	void sleep();
	void idleSleep();
	void wakeup();

private:
	SystemWatch();

	static SystemWatch* instance_;
};


class SystemWatchImpl : public QObject
{
	Q_OBJECT

protected:
	SystemWatchImpl();
		
signals:
	void sleep();
	void idleSleep();
	void wakeup();
};

#endif
