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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psiapplication.h"
#include "resourcemenu.h"

#include <QSessionManager>

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#include "CocoaUtilities/CocoaTrayClick.h"
#endif

#ifdef HAVE_X11
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <QDesktopWidget>
#include <QEvent>
#include <QX11Info>
#ifdef HAVE_QT5
# include <xcb/xcb.h>
# include <QAbstractNativeEventFilter>
#endif

// Atoms required for monitoring the freedesktop.org notification area
#ifdef HAVE_QT5
static xcb_atom_t manager_atom = 0;
static xcb_atom_t tray_selection_atom = 0;
xcb_window_t root_window = 0;
xcb_window_t tray_owner = None;
//static Atom atom_KdeNetUserTime;
static xcb_atom_t kde_net_wm_user_time = 0;
QAbstractNativeEventFilter *_xcbEventFilter = 0;

#else
static Atom manager_atom = 0;
static Atom tray_selection_atom = 0;
Window root_window = 0;
Window tray_owner = None;
//static Atom atom_KdeNetUserTime;
static Atom kde_net_wm_user_time = 0;
#endif



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

#ifdef Q_OS_WIN
#include "systemwatch/systemwatch_win.h"
#endif

// mblsha:
// currently this file contains some Anti-"focus steling prevention" code by
// Lubos Lunak (l.lunak@kde.org)
//
// This should resolve all bugs with KWin3 and old Qt, but maybe it'll be useful for
// other window managers?

#ifdef HAVE_X11
# ifdef HAVE_QT5
class XcbEventFiter : public QAbstractNativeEventFilter
{
	PsiApplication *app;

public:
	XcbEventFiter(PsiApplication *app) : app(app) {}

	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *) Q_DECL_OVERRIDE
	{
		if (eventType == "xcb_generic_event_t") {
			return app->xcbEventFilter(message);
		}
		return false;
	}
};

void setTrayOwnerWindow()
{
	/* This code is basically trying to mirror what happens in
	 * eggtrayicon.c:egg_tray_icon_update_manager_window()
	 */

	uint32_t events = 0;
	// ignore events from the old tray owner
	if (tray_owner != None)
	{
		xcb_change_window_attributes(QX11Info::connection(), tray_owner, XCB_CW_EVENT_MASK, &events);
	}

	// obtain the Window handle for the new tray owner
	xcb_grab_server(QX11Info::connection());
	xcb_get_selection_owner_cookie_t tray_owner_cookie =
			xcb_get_selection_owner(QX11Info::connection(), tray_selection_atom);
	xcb_get_selection_owner_reply_t *tray_owner_reply =
			xcb_get_selection_owner_reply(QX11Info::connection(), tray_owner_cookie, 0);
	tray_owner = tray_owner_reply? tray_owner_reply->owner :  0;
	if (tray_owner) {
		events = XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_PROPERTY_CHANGE;
		xcb_change_window_attributes(QX11Info::connection(), tray_owner, XCB_CW_EVENT_MASK,
			&events);
	}


	// we have to be able to spot DestroyNotify messages on the tray owner
	if (tray_owner != None) {

	}
	xcb_ungrab_server(QX11Info::connection());
	xcb_flush(QX11Info::connection());
}
# else
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
# endif
#endif

//----------------------------------------------------------------------------
// PsiMacStyle
//----------------------------------------------------------------------------

#if defined (Q_OS_MAC) && !defined (HAVE_QT5)
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
		// Since Qt 4.6, it's better to use QAction::setIconVisibleInMenu()
		// extern void qt_mac_set_menubar_icons(bool b); // qmenu_mac.cpp
		// qt_mac_set_menubar_icons(false);
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
:	QApplication(argc, argv, GUIenabled)
{
	init(GUIenabled);
#ifdef Q_OS_MAC
	connect(CocoaTrayClick::instance(), SIGNAL(trayClicked()), this, SIGNAL(dockActivated()));
#endif
}

PsiApplication::~PsiApplication()
{
#if defined(HAVE_X11) && defined(HAVE_QT5)
	delete _xcbEventFilter;
#endif
}

void PsiApplication::init(bool GUIenabled)
{
	Q_UNUSED(GUIenabled);
#ifdef Q_OS_MAC
#ifdef HAVE_QT5
	setAttribute(Qt::AA_DontShowIconsInMenus, true);
#else
	setStyle(new PsiMacStyle());
#endif
#endif

#ifdef HAVE_X11
    if ( GUIenabled && QX11Info::isPlatformX11() ) {
		const int max = 20;
		char* names[max];
		int n = 0;
		char buf[32];
		// make a note of the window handle for the root window
		root_window = QX11Info::appRootWindow();

#ifdef HAVE_QT5
		xcb_atom_t* atoms[max];
		xcb_intern_atom_cookie_t cookies[max];


		_xcbEventFilter = new XcbEventFiter(this);
		// get the selection type we'll use to locate the notification tray
		snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", QX11Info::appScreen());
		xcb_connection_t *xcb = QX11Info::connection();

		//atoms[n] = &atom_KdeNetUserTime;
		//names[n++] = (char *) "_KDE_NET_USER_TIME";

		atoms[n] = &kde_net_wm_user_time;
		names[n++] = (char *) "_NET_WM_USER_TIME";
		atoms[n] = &manager_atom;
		names[n++] = (char *) "MANAGER";
		atoms[n] = &tray_selection_atom;
		names[n++] = buf;

		for (int i = 0; i < n; i++ ) {
			cookies[i] = xcb_intern_atom(xcb, 0, strlen(names[i]), names[i]);
		}
		for (int i = 0; i < n; i++ ) {
			xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(xcb, cookies[i], 0);
			*atoms[i] = reply? reply->atom : 0;
		}


		xcb_get_window_attributes_cookie_t wa_cookie = xcb_get_window_attributes(xcb, root_window);
		xcb_get_window_attributes_reply_t *wa_reply = xcb_get_window_attributes_reply(xcb, wa_cookie, 0);
		if (wa_reply) {
			uint32_t event_mask = wa_reply->your_event_mask;
			event_mask |= XCB_EVENT_MASK_STRUCTURE_NOTIFY;
			xcb_change_window_attributes(xcb, root_window, XCB_CW_EVENT_MASK,
				&event_mask);
		}
		setTrayOwnerWindow();
#else
		Atom* atoms[max];
		Atom atoms_return[max];

		Display *dsp = QX11Info::display();

		// get the selection type we'll use to locate the notification tray
		snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", XScreenNumberOfScreen( XDefaultScreenOfDisplay(dsp) ));

		//atoms[n] = &atom_KdeNetUserTime;
		//names[n++] = (char *) "_KDE_NET_USER_TIME";

		atoms[n] = &kde_net_wm_user_time;
		names[n++] = (char *) "_NET_WM_USER_TIME";
		atoms[n] = &manager_atom;
		names[n++] = (char *) "MANAGER";
		atoms[n] = &tray_selection_atom;
		names[n++] = buf;

		XInternAtoms( dsp, names, n, false, atoms_return );

		for (int i = 0; i < n; i++ ) {
			*atoms[i] = atoms_return[i];
		}

		XWindowAttributes attr;

		// this is actually futile, since Qt overrides it at some
		// unknown point in the near future.
		XGetWindowAttributes(dsp, root_window, &attr);
		XSelectInput(dsp, root_window, attr.your_event_mask | StructureNotifyMask);
		setTrayOwnerWindow(dsp);
#endif
	}
#endif
}

bool PsiApplication::notify(QObject *receiver, QEvent *event)
{
#ifdef HAVE_X11
    if( event->type() == QEvent::Show && receiver->isWidgetType() && QX11Info::isPlatformX11())
	{
		QWidget* w = static_cast< QWidget* >( receiver );
		if( w->isTopLevel() && qt_x_last_input_time != CurrentTime ) // CurrentTime means no input event yet
#ifdef HAVE_QT5
			xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE,
								w->winId(), kde_net_wm_user_time, XCB_ATOM_CARDINAL,
								32, 1, (unsigned char*)&qt_x_last_input_time);
#else
			XChangeProperty( QX11Info::display(), w->winId(), kde_net_wm_user_time, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char*)&qt_x_last_input_time, 1 );
#endif
	}
	if( event->type() == QEvent::Hide && receiver->isWidgetType())
	{
		QWidget* w = static_cast< QWidget* >( receiver );
		if( w->isTopLevel() && w->winId() != 0 )
#ifdef HAVE_QT5
			xcb_delete_property(QX11Info::connection(), w->winId(), kde_net_wm_user_time);
#else
			XDeleteProperty( QX11Info::display(), w->winId(), kde_net_wm_user_time );
#endif
	}
#endif
	return QApplication::notify(receiver, event);
}


#ifdef HAVE_X11
# ifdef HAVE_QT5
bool PsiApplication::xcbEventFilter(void *event)
{
	xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(event);
	switch (ev->response_type) {
	case XCB_CLIENT_MESSAGE:
	{
		xcb_client_message_event_t *msg = static_cast<xcb_client_message_event_t *>(event);
		if (msg->window == root_window && msg->type == manager_atom)
		{
			// A new notification area application has
			// announced its presence
			setTrayOwnerWindow();
			newTrayOwner();
		}
		break;
	}
	case XCB_DESTROY_NOTIFY:
	{
		xcb_destroy_notify_event_t *dn = static_cast<xcb_destroy_notify_event_t *>(event);
		if (dn->event == tray_owner)
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
	}
	case XCB_BUTTON_PRESS:
	case XCB_KEY_PRESS:
	{

		if( ev->response_type == XCB_BUTTON_PRESS )
			qt_x_last_input_time = static_cast<xcb_button_press_event_t *>(event)->time;
		else // XCB_KEY_PRESS
			qt_x_last_input_time = static_cast<xcb_key_press_event_t *>(event)->time;
		QWidget *w = activeWindow();
		if( w ) {
			xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE,
								w->winId(), kde_net_wm_user_time, XCB_ATOM_CARDINAL,
								32, 1, (unsigned char*)&qt_x_last_input_time);
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
	return false;
}

# else
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
# endif
#endif

#ifdef Q_OS_MAC
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

void PsiApplication::commitData(QSessionManager& manager)
{
	Q_UNUSED(manager);
	emit forceSavePreferences();
}
