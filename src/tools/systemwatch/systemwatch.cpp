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

#include "systemwatch.h"
#if defined(Q_OS_MAC)
	#include "systemwatch_mac.h"
#elif defined(Q_OS_WIN32)
	#include "systemwatch_win.h"
#else
	#include "systemwatch_unix.h"
#endif

#include <QApplication>

SystemWatch::SystemWatch()
	: QObject(qApp)
{
	SystemWatchImpl* impl;
#if defined(Q_WS_MAC)
	impl = MacSystemWatch::instance();
#elif defined(Q_WS_WIN)
	impl = WinSystemWatch::instance();
#else
	impl = UnixSystemWatch::instance();
#endif
	connect(impl,SIGNAL(sleep()),this,SIGNAL(sleep()));
	connect(impl,SIGNAL(idleSleep()),this,SIGNAL(idleSleep()));
	connect(impl,SIGNAL(wakeup()),this,SIGNAL(wakeup()));
}


SystemWatch* SystemWatch::instance()
{
	if (!instance_) 
		instance_ = new SystemWatch();

	return instance_;
}


SystemWatch* SystemWatch::instance_ = 0;

SystemWatchImpl::SystemWatchImpl()
	: QObject(qApp)
{
}
