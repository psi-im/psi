/*
 * psiapplication.cpp - subclass of QApplication to do some workarounds
 * Copyright (C) 2003  Michail Pishchagin
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

#include "psiapplication.h"
#include "resourcemenu.h"

#ifdef Q_WS_MAC
#include <Carbon/Carbon.h>
#endif

#ifdef Q_WS_X11
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/SM/SMlib.h>
#include <QX11Info>
#include <QDesktopWidget>
#include <QEvent>

// Atoms required for monitoring the freedesktop.org notification area
static Atom manager_atom = 0;
static Atom tray_selection_atom = 0;
Window root_window = 0;
Window tray_owner = None;

//static Atom atom_KdeNetUserTime;
static Atom kde_net_wm_user_time = 0;

Time   qt_x_last_input_time = CurrentTime;
//extern Time qt_x_time;

#ifdef KeyPress
#ifndef FIXX11H_KeyPress
#define FIXX11H_KeyPress
const int XKeyPress = KeyPress;
#undef KeyPress
const int KeyPress = XKeyPress;
#endif
#undef KeyPress
#endif
#endif

#ifdef Q_WS_WIN
#include "systemwatch_win.h"
#endif

// mblsha:
// currently this file contains some Anti-"focus steling prevention" code by
// Lubos Lunak (l.lunak@kde.org)
//
// This should resolve all bugs with KWin3 and old Qt, but maybe it'll be useful for
// other window managers?

#ifdef Q_WS_X11
//#undef Q_WS_X11
#endif

#ifdef Q_WS_X11
void setTrayOwnerWindow(Display *dsp)
{
	/* This code is basically trying to mirror what happens in
	 * eggtrayicon.c:egg_tray_icon_update_manager_window()
	 */
	// ignore events from the old tray owner
	if (tray_owner != None)
	{
		XSelectInput(dsp, tray_owner, 0);
	}

	// obtain the Window handle for the new tray owner
	XGrabServer(dsp);
	tray_owner = XGetSelectionOwner(dsp, tray_selection_atom);

	// we have to be able to spot DestroyNotify messages on the tray owner
	if (tray_owner != None)
	{
		XSelectInput(dsp, tray_owner, StructureNotifyMask|PropertyChangeMask);
	}
	XUngrabServer(dsp);
	XFlush(dsp);
}

#endif

//----------------------------------------------------------------------------
// PsiMacStyle
//----------------------------------------------------------------------------

#ifdef Q_WS_MAC
#include <QMacStyle>
#include <QStyleOptionMenuItem>

/**
 * Custom QStyle that helps to get rid of icons in all kinds of menus
 * on Mac OS X.
 */
class PsiMacStyle : public QMacStyle
{
public:
	PsiMacStyle() 
	{
		extern void qt_mac_set_menubar_icons(bool b); // qmenu_mac.cpp
		qt_mac_set_menubar_icons(false);
	}
	
	void drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p, const QWidget *w) const
	{
		if (disableIconsForMenu(w) && ce == QStyle::CE_MenuItem) {
			if (const QStyleOptionMenuItem *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)) {
				QStyleOptionMenuItem newopt(*mi);
				newopt.maxIconWidth = 0;
				newopt.icon = QIcon();
				QMacStyle::drawControl(ce, &newopt, p, w);
				return;
		        }
		}

		QMacStyle::drawControl(ce, opt, p, w);
	}
	
	QSize sizeFromContents(ContentsType ct, const QStyleOption *opt, const QSize &csz, const QWidget *widget) const
	{
		if (disableIconsForMenu(widget) && ct == QStyle::CT_MenuItem) {
			if (const QStyleOptionMenuItem *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)) {
				QStyleOptionMenuItem newopt(*mi);
				newopt.maxIconWidth = 0;
				newopt.icon = QIcon();
				return QMacStyle::sizeFromContents(ct, &newopt, csz, widget);
		        }
		}

		return QMacStyle::sizeFromContents(ct, opt, csz, widget);
	}
		
private:
	/**
	 * This function provides means to override the icon-disabling
	 * behavior. For instance, ResourceMenu's items are shown
	 * with icons on screen.
	 */
	bool disableIconsForMenu(const QWidget *menu) const
	{
		return !menu || !dynamic_cast<const ResourceMenu*>(menu);
	}
};
#endif

//----------------------------------------------------------------------------
// PsiApplication
//----------------------------------------------------------------------------

PsiApplication::PsiApplication(int &argc, char **argv, bool GUIenabled)
: QApplication(argc, argv, GUIenabled)
{
	init(GUIenabled);
}

PsiApplication::~PsiApplication()
{
}

void PsiApplication::init(bool GUIenabled)
{
	Q_UNUSED(GUIenabled);
#ifdef Q_WS_MAC
	setStyle(new PsiMacStyle());
#endif
	
#ifdef Q_WS_X11
	if ( GUIenabled ) {
		const int max = 20;
		Atom* atoms[max];
		char* names[max];
		Atom atoms_return[max];
		int n = 0;

		//atoms[n] = &atom_KdeNetUserTime;
		//names[n++] = (char *) "_KDE_NET_USER_TIME";

		atoms[n] = &kde_net_wm_user_time;
		names[n++] = (char *) "_NET_WM_USER_TIME";
		atoms[n] = &manager_atom;
		names[n++] = (char *) "MANAGER";

		Display *dsp = QX11Info::display();

		XInternAtoms( dsp, names, n, false, atoms_return );

		for (int i = 0; i < n; i++ )
			*atoms[i] = atoms_return[i];

		// get the selection type we'll use to locate the notification tray
		char buf[32];
		snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", XScreenNumberOfScreen( XDefaultScreenOfDisplay(dsp) ));
		tray_selection_atom = XInternAtom(dsp, buf, false);

		// make a note of the window handle for the root window
		root_window = QApplication::desktop()->winId();

		XWindowAttributes attr;

		// this is actually futile, since Qt overrides it at some
		// unknown point in the near future.
		XGetWindowAttributes(dsp, root_window, &attr);
		XSelectInput(dsp, root_window, attr.your_event_mask | StructureNotifyMask);

		setTrayOwnerWindow(dsp);
	}
#endif
}

bool PsiApplication::notify(QObject *receiver, QEvent *event)
{
#ifdef Q_WS_X11
	if( event->type() == QEvent::Show && receiver->isWidgetType())
	{
		QWidget* w = static_cast< QWidget* >( receiver );
		if( w->isTopLevel() && qt_x_last_input_time != CurrentTime ) // CurrentTime means no input event yet
			XChangeProperty( QX11Info::display(), w->winId(), kde_net_wm_user_time, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char*)&qt_x_last_input_time, 1 );
	}
	if( event->type() == QEvent::Hide && receiver->isWidgetType())
	{
		QWidget* w = static_cast< QWidget* >( receiver );
		if( w->isTopLevel() && w->winId() != 0 )
			XDeleteProperty( QX11Info::display(), w->winId(), kde_net_wm_user_time );
	}
#endif
	return QApplication::notify(receiver, event);
}

#ifdef Q_WS_X11
bool PsiApplication::x11EventFilter( XEvent *_event )
{
	switch ( _event->type ) {

		case ClientMessage:
			if (_event->xclient.window == root_window && _event->xclient.message_type == manager_atom)
			{
				// A new notification area application has
				// announced its presence
				setTrayOwnerWindow(_event->xclient.display);
				newTrayOwner();
			}
			break;
			
		case DestroyNotify:
			if (_event->xdestroywindow.event == tray_owner)
			{
				// there is now no known notification area.
				// We're still looking out for the MANAGER
				// message sent to the root window, at which
				// point we'll have another look to see
				// whether a notification area is available.
				tray_owner = 0;
				trayOwnerDied();
			}
			break;

		case ButtonPress:
	 	case XKeyPress:
		{
			if( _event->type == ButtonPress )
				qt_x_last_input_time = _event->xbutton.time;
			else // KeyPress
				qt_x_last_input_time = _event->xkey.time;
			QWidget *w = activeWindow();
			if( w ) {
				XChangeProperty( QX11Info::display(), w->winId(), kde_net_wm_user_time, XA_CARDINAL,
						 32, PropModeReplace, (unsigned char*)&qt_x_last_input_time, 1 );
				/*timeval tv;
				gettimeofday( &tv, NULL );
				unsigned long now = tv.tv_sec * 10 + tv.tv_usec / 100000;
				XChangeProperty(qt_xdisplay(), w->winId(),
						atom_KdeNetUserTime, XA_CARDINAL,
						32, PropModeReplace, (unsigned char *)&now, 1);*/
			}
			break;
		}

		default:
			break;
	}

	// process the event normally
	return false;
}
#endif

#ifdef Q_WS_MAC
bool PsiApplication::macEventFilter( EventHandlerCallRef, EventRef inEvent )
{
	UInt32 eclass = GetEventClass(inEvent);
	int etype = GetEventKind(inEvent);
	if(eclass == 'eppc' && etype == kEventAppleEvent) {
		dockActivated();
	}
	return false;
}
#endif

#ifdef Q_WS_WIN
bool PsiApplication::winEventFilter(MSG* msg, long* result)
{
	if (msg->message == WM_POWERBROADCAST || msg->message == WM_QUERYENDSESSION) {
		return static_cast<WinSystemWatch*>(SystemWatch::instance())->processWinEvent(msg, result);
	}
	return false;
}
#endif

void PsiApplication::commitData(QSessionManager& manager)
{
	emit forceSavePreferences();
}
