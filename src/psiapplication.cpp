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
#include <xcb/xcb.h>
#include <QAbstractNativeEventFilter>

// Atoms required for monitoring the freedesktop.org notification area
xcb_window_t root_window = 0;
static xcb_atom_t kde_net_wm_user_time = 0;
QAbstractNativeEventFilter *_xcbEventFilter = 0;


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
// currently this file contains some Anti-"focus stealing prevention" code by
// Lubos Lunak (l.lunak@kde.org)
//
// This should resolve all bugs with KWin3 and old Qt, but maybe it'll be useful for
// other window managers?

#ifdef HAVE_X11
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
#endif

//----------------------------------------------------------------------------
// PsiApplication
//----------------------------------------------------------------------------

PsiApplication::PsiApplication(int &argc, char **argv, bool GUIenabled)
:    QApplication(argc, argv, GUIenabled)
{
    init(GUIenabled);
#ifdef Q_OS_MAC
    connect(CocoaTrayClick::instance(), SIGNAL(trayClicked()), this, SIGNAL(dockActivated()));
#endif
}

PsiApplication::~PsiApplication()
{
#if defined(HAVE_X11)
    delete _xcbEventFilter;
#endif
}

void PsiApplication::init(bool GUIenabled)
{
    Q_UNUSED(GUIenabled);
#ifdef Q_OS_MAC
    setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif

#ifdef HAVE_X11
    if ( GUIenabled && QX11Info::isPlatformX11() ) {
        const int max = 20;
        char* names[max];
        int n = 0;
        // make a note of the window handle for the root window
        root_window = QX11Info::appRootWindow();

        xcb_atom_t* atoms[max];
        xcb_intern_atom_cookie_t cookies[max];

        _xcbEventFilter = new XcbEventFiter(this);
        xcb_connection_t *xcb = QX11Info::connection();

        atoms[n] = &kde_net_wm_user_time;
        names[n++] = (char *) "_NET_WM_USER_TIME";

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
        installNativeEventFilter(_xcbEventFilter);
    }
#endif
}

bool PsiApplication::notify(QObject *receiver, QEvent *event)
{
#ifdef HAVE_X11
    if( event->type() == QEvent::Show && receiver->isWidgetType()
            && QX11Info::isPlatformX11())
    {
        QWidget* w = static_cast< QWidget* >( receiver );
        if( w->isTopLevel() && qt_x_last_input_time != CurrentTime ) // CurrentTime means no input event yet
            xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE,
                                w->winId(), kde_net_wm_user_time, XCB_ATOM_CARDINAL,
                                32, 1, (unsigned char*)&qt_x_last_input_time);
    }
    if( event->type() == QEvent::Hide && receiver->isWidgetType()
            && QX11Info::isPlatformX11())
    {
        QWidget* w = static_cast< QWidget* >( receiver );
        if( w->isTopLevel() && w->winId() != 0 )
            xcb_delete_property(QX11Info::connection(), w->winId(), kde_net_wm_user_time);
    }
#endif
    return QApplication::notify(receiver, event);
}


#ifdef HAVE_X11
bool PsiApplication::xcbEventFilter(void *event)
{
    xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(event);
    switch (ev->response_type) {
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
