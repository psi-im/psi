/*
 * systemwatch_mac.cpp - Detect changes in the system state (Windows).
 * Copyright (C) 2005  James Chaldecott
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

#include "systemwatch_win.h"

#include <qwidget.h>
#include <qt_windows.h>

// These defines come from Microsoft Platform SDK, August 2005
#if(WINVER >= 0x0400)
# ifndef WM_POWERBROADCAST
#  define WM_POWERBROADCAST                0x0218
# endif // WM_POWERBROADCAST
# ifndef _WIN32_WCE
#  ifndef PBT_APMQUERYSUSPEND
#   define PBT_APMQUERYSUSPEND             0x0000
#  endif // PBT_APMQUERYSUSPEND
#  ifndef PBT_APMQUERYSTANDBY
#   define PBT_APMQUERYSTANDBY             0x0001
#  endif // PBT_APMQUERYSTANDBY
#  ifndef PBT_APMQUERYSUSPENDFAILED
#   define PBT_APMQUERYSUSPENDFAILED       0x0002
#  endif // PBT_APMQUERYSUSPENDFAILED
#  ifndef PBT_APMQUERYSTANDBYFAILED
#   define PBT_APMQUERYSTANDBYFAILED       0x0003
#  endif // PBT_APMQUERYSTANDBYFAILED
#  ifndef PBT_APMSUSPEND
#   define PBT_APMSUSPEND                  0x0004
#  endif // PBT_APMSUSPEND
#  ifndef PBT_APMSTANDBY
#   define PBT_APMSTANDBY                  0x0005
#  endif // PBT_APMSTANDBY
#  ifndef PBT_APMRESUMECRITICAL
#   define  PBT_APMRESUMECRITICAL          0x0006
#  endif // PBT_APMRESUMECRITICAL
#  ifndef PBT_APMRESUMESUSPEND
#   define PBT_APMRESUMESUSPEND            0x0007
#  endif // PBT_APMRESUMESUSPEND
#  ifndef PBT_APMRESUMESTANDBY
#   define PBT_APMRESUMESTANDBY            0x0008
#  endif // PBT_APMRESUMESTANDBY
#  ifndef PBTF_APMRESUMEFROMFAILURE
#   define PBTF_APMRESUMEFROMFAILURE       0x00000001
#  endif // PBTF_APMRESUMEFROMFAILURE
#  ifndef PBT_APMBATTERYLOW
#   define PBT_APMBATTERYLOW               0x0009
#  endif // PBT_APMBATTERYLOW
#  ifndef PBT_APMPOWERSTATUSCHANGE
#   define PBT_APMPOWERSTATUSCHANGE        0x000A
#  endif // PBT_APMPOWERSTATUSCHANGE
#  ifndef PBT_APMOEMEVENT
#   define PBT_APMOEMEVENT                 0x000B
#  endif // PBT_APMOEMEVENT
#  ifndef PBT_APMRESUMEAUTOMATIC
#   define PBT_APMRESUMEAUTOMATIC          0x0012
#  endif // PBT_APMRESUMEAUTOMATIC
# endif // _WIN32_WCE
#endif // WINVER >= 0x0400

// -----------------------------------------------------------------------------
// WinSystemWatchPrivate
// -----------------------------------------------------------------------------

class WinSystemWatch::WinSystemWatchPrivate : public QWidget
{
	Q_OBJECT

public:
	WinSystemWatchPrivate( ) : QWidget( 0 ) { }
    ~WinSystemWatchPrivate() { }

	bool winEvent( MSG *, long * ); 

signals:
	void sleep();
	void wakeup();
};


bool WinSystemWatch::WinSystemWatchPrivate::winEvent( MSG *m, long *result ) 
{
	if(WM_POWERBROADCAST == m->message) {
		switch (m->wParam) {
			case PBT_APMSUSPEND:
				emit sleep();
				break;

			case PBT_APMRESUMESUSPEND:
				emit wakeup();
				break;

			case PBT_APMRESUMECRITICAL:
				// The system previously went into SUSPEND state (suddenly)
				// without sending PBT_APMSUSPEND.  Net connections are
				// probably invalid.  Not sure what to do about this.
				// Maybe:
				emit sleep();
				emit wakeup();
				break;

			case PBT_APMQUERYSUSPEND:
				// TODO: Check if file transfers are running, and don't go
				// to sleep if there are.  To refuse to suspend, we somehow
				// need to return BROADCAST_QUERY_DENY from the actual
				// windows procedure.
				break;
		}
	}
	else if (WM_QUERYENDSESSION == m->message) {
		// TODO : If we allow the user to cancel suspend if they
		// are doing a file transfer, we should probably also give
		// them the chance to cancel a shutdown or log-off
	}
	return QWidget::winEvent( m, result );
}


// -----------------------------------------------------------------------------
// WinSystemWatch
// -----------------------------------------------------------------------------

WinSystemWatch::WinSystemWatch() 
{
	d = new WinSystemWatchPrivate();
	connect(d,SIGNAL(sleep()),this,SIGNAL(sleep()));
	connect(d,SIGNAL(wakeup()),this,SIGNAL(wakeup()));
}

WinSystemWatch::~WinSystemWatch()
{
	disconnect(d,SIGNAL(sleep()),this,SIGNAL(sleep()));
	disconnect(d,SIGNAL(wakeup()),this,SIGNAL(wakeup()));
	delete d;
}

WinSystemWatch* WinSystemWatch::instance()
{
	if (!instance_) 
		instance_ = new WinSystemWatch();

	return instance_;
}

WinSystemWatch* WinSystemWatch::instance_ = 0;

// -----------------------------------------------------------------------------

#include "systemwatch_win.moc"

