/*
 * advwidget.cpp - AdvancedWidget template class
 * Copyright (C) 2005-2007  Michail Pishchagin
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

#include <qglobal.h>
#ifdef Q_OS_WIN
#if __GNUC__ >= 3
#	define WINVER 0x0500
#endif
#include <windows.h>
#include <winuser.h>
#endif

#include "advwidget.h"

#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QDesktopWidget>
#include <QDebug>

#ifdef Q_WS_X11
#include<X11/Xutil.h>
#include <QX11Info>
#endif

// TODO: Make use of KDE taskbar flashing support

//----------------------------------------------------------------------------
// AdvancedWidgetShared
//----------------------------------------------------------------------------

class AdvancedWidgetShared : public QObject
{
	Q_OBJECT
public:
	AdvancedWidgetShared();
	~AdvancedWidgetShared();
};

AdvancedWidgetShared::AdvancedWidgetShared()
	: QObject(qApp)
{
}

AdvancedWidgetShared::~AdvancedWidgetShared()
{
}

static AdvancedWidgetShared *advancedWidgetShared = 0;

//----------------------------------------------------------------------------
// GAdvancedWidget::Private
//----------------------------------------------------------------------------

class GAdvancedWidget::Private : public QObject
{
	Q_OBJECT
public:
	Private(QWidget *parent);

	static int  stickAt;
	static bool stickToWindows;
	static bool stickEnabled;

	QWidget *parentWidget;
	bool flashing_;

	bool flashing() const;
	void doFlash(bool on);
	void posChanging(int *x, int *y, int *width, int *height);
	void moveEvent(QMoveEvent *e);

private:
	QRect newGeometry;

private slots:
	void updateGeometry();
};

int  GAdvancedWidget::Private::stickAt        = 5;
bool GAdvancedWidget::Private::stickToWindows = true;
bool GAdvancedWidget::Private::stickEnabled   = true;

GAdvancedWidget::Private::Private(QWidget *parent)
	: QObject(parent)
	, parentWidget(parent)
	, flashing_(false)
{
	if ( !advancedWidgetShared )
		advancedWidgetShared = new AdvancedWidgetShared();

	parentWidget = parent;
}

void GAdvancedWidget::Private::posChanging(int *x, int *y, int *width, int *height)
{
	if ( stickAt <= 0                    ||
	    !stickEnabled                    ||
	    !parentWidget->isTopLevel()      ||
	     parentWidget->isMaximized()     ||
	    !parentWidget->updatesEnabled() )
	{
		return;
	}

	QWidget *p = parentWidget;
	if ( p->pos() == QPoint(*x, *y) &&
	     p->frameSize() == QSize(*width, *height) )
		return;

	bool resizing = p->frameSize() != QSize(*width, *height);

	QDesktopWidget *desktop = qApp->desktop();
	QWidgetList list;

	if ( stickToWindows )
		list = QApplication::topLevelWidgets();

	foreach(QWidget *w, list) {
		QRect rect;
		bool dockWidget = false;

		if ( w->windowType() == Qt::Desktop )
			rect = ((QDesktopWidget *)w)->availableGeometry((QWidget *)parent());
		else {
			if ( w == p ||
			     desktop->screenNumber(p) != desktop->screenNumber(w) )
				continue;

			if ( !w->isVisible() )
				continue;

			// we want for widget to stick to outer edges of another widget, so
			// we'll change the rect to what it'll snap
			rect = QRect(w->frameGeometry().bottomRight(), w->frameGeometry().topLeft());
			dockWidget = true;
		}

		if ( *x != p->x() )
		if ( *x <= rect.left() + stickAt &&
		     *x >  rect.left() - stickAt ) {
			if ( !dockWidget ||
			     (p->frameGeometry().bottom() >= rect.bottom() &&
			      p->frameGeometry().top() <= rect.top()) ) {
				*x = rect.left();
				if ( resizing )
					*width = p->frameSize().width() + p->x() - *x;
			}
		}

		if ( *x + *width >= rect.right() - stickAt &&
		     *x + *width <= rect.right() + stickAt ) {
			if ( !dockWidget ||
			     (p->frameGeometry().bottom() >= rect.bottom() &&
			      p->frameGeometry().top() <= rect.top()) ) {
				if ( resizing )
					*width = rect.right() - *x + 1;
				else
					*x = rect.right() - *width + 1;
			}
		}

		if ( *y != p->y() )
		if ( *y <= rect.top() + stickAt &&
		     *y >  rect.top() - stickAt ) {
			if ( !dockWidget ||
			     (p->frameGeometry().right() >= rect.right() &&
			      p->frameGeometry().left() <= rect.left()) ) {
				*y = rect.top();
				if ( resizing )
					*height = p->frameSize().height() + p->y() - *y;
			}
		}

		if ( *y + *height >= rect.bottom() - stickAt &&
		     *y + *height <= rect.bottom() + stickAt ) {
			if ( !dockWidget ||
			     (p->frameGeometry().right() >= rect.right() &&
			      p->frameGeometry().left() <= rect.left()) ) {
				if ( resizing )
					*height = rect.bottom() - *y + 1;
				else
					*y = rect.bottom() - *height + 1;
			}
		}
	}
}

bool GAdvancedWidget::Private::flashing() const
{
	return flashing_;
}

void GAdvancedWidget::Private::doFlash(bool yes)
{
	flashing_ = yes;
	if (parentWidget->window() != parentWidget)
		return;

#ifdef Q_WS_WIN
	FLASHWINFO fwi;
	fwi.cbSize = sizeof(fwi);
	fwi.hwnd = parentWidget->winId();
	if (yes) {
		fwi.dwFlags = FLASHW_ALL | FLASHW_TIMER;
		fwi.dwTimeout = 0;
		fwi.uCount = 5;
	}
	else {
		fwi.dwFlags = FLASHW_STOP;
		fwi.uCount = 0;
	}
	FlashWindowEx(&fwi);

#elif defined( Q_WS_X11 )
	static Atom demandsAttention = None;
	static Atom wmState = None;


    /* Xlib-based solution */
	// adopted from http://www.qtforum.org/article/12334/Taskbar-flashing.html
	// public domain by Marcin Jakubowski
    Display *xdisplay = QX11Info::display();
    Window rootwin = QX11Info::appRootWindow();

	if (demandsAttention == None)
    	demandsAttention = XInternAtom(xdisplay, "_NET_WM_STATE_DEMANDS_ATTENTION", true);
	if (wmState == None)
		wmState = XInternAtom(xdisplay, "_NET_WM_STATE", true);

    XEvent e;
    e.xclient.type = ClientMessage;
    e.xclient.message_type = wmState;
    e.xclient.display = xdisplay;
    e.xclient.window = parentWidget->winId();
    e.xclient.format = 32;
    e.xclient.data.l[1] = demandsAttention;
    e.xclient.data.l[2] = 0l;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;

    if (yes) {
        e.xclient.data.l[0] = 1;
    }
    else {
        e.xclient.data.l[0] = 0;
    }
    XSendEvent(xdisplay, rootwin, False, (SubstructureRedirectMask | SubstructureNotifyMask), &e);

#else
	Q_UNUSED(yes)
#endif
}

void GAdvancedWidget::Private::moveEvent(QMoveEvent *)
{
	if (!parentWidget->isTopLevel())
		return;
#ifdef Q_WS_MAC
	QRect r = qApp->desktop()->availableGeometry(parentWidget);
	QRect g = parentWidget->frameGeometry();

	int margin = 5;

	if (g.top() < r.top())
		g.moveTo(g.x(), r.top());

	if (g.right() < r.left() + margin)
		g.moveTo(r.left() + margin - g.width(), g.y());

	if (g.left() > r.right() - margin)
		g.moveTo(r.right() - margin, g.y());

	if (g.top() > r.bottom() - margin)
		g.moveTo(g.x(), r.bottom() - margin);

	newGeometry = g;
	QTimer::singleShot(0, this, SLOT(updateGeometry()));
#endif
}

void GAdvancedWidget::Private::updateGeometry()
{
	QWidget *w = (QWidget *)parent();
	w->move(newGeometry.topLeft());
}

//----------------------------------------------------------------------------
// GAdvancedWidget
//----------------------------------------------------------------------------

GAdvancedWidget::GAdvancedWidget(QWidget *parent)
	: QObject(parent)
{
	d = new Private(parent);
}

#ifdef Q_OS_WIN
bool GAdvancedWidget::winEvent(MSG* msg, long* result)
{
	if ( msg->message == WM_WINDOWPOSCHANGING ) {
		WINDOWPOS *wpos = (WINDOWPOS *)msg->lParam;

		d->posChanging(&wpos->x, &wpos->y, &wpos->cx, &wpos->cy);

		result = 0;
		return true;
	}

	return false;
}
#endif

void GAdvancedWidget::restoreSavedGeometry(QRect savedGeometry)
{
	QRect geom = savedGeometry;
	QDesktopWidget *pdesktop = QApplication::desktop();
	int nscreen = pdesktop->screenNumber(geom.topLeft());
	QRect r = pdesktop->screenGeometry(nscreen);

	// if the coordinates are out of the desktop bounds, reset to the top left
	int pad = 10;
	if((geom.width() + pad * 2) > r.width())
		geom.setWidth(r.width() - pad * 2);
	if((geom.height() + pad * 2) > r.height())
		geom.setHeight(r.height() - pad * 2);
	if(geom.left() < r.left())
		geom.moveLeft(r.left());
	if(geom.right() >= r.right())
		geom.moveRight(r.right() - 1);
	if(geom.top() < r.top())
		geom.moveTop(r.top());
	if(geom.bottom() >= r.bottom())
		geom.moveBottom(r.bottom() - 1);

	d->parentWidget->move(geom.topLeft());
	d->parentWidget->resize(geom.size());
}

bool GAdvancedWidget::flashing() const
{
	return d->flashing();
}

void GAdvancedWidget::doFlash(bool on)
{
	d->doFlash( on );
}

void GAdvancedWidget::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::ActivationChange ||
	    event->type() == QEvent::WindowStateChange)
	{
		if (d->parentWidget->isActiveWindow()) {
			doFlash(false);
		}
	}
}

int GAdvancedWidget::stickAt()
{
	return Private::stickAt;
}

void GAdvancedWidget::setStickAt(int val)
{
	Private::stickAt = val;
}

bool GAdvancedWidget::stickToWindows()
{
	return Private::stickToWindows;
}

void GAdvancedWidget::setStickToWindows(bool val)
{
	Private::stickToWindows = val;
}

bool GAdvancedWidget::stickEnabled()
{
	return Private::stickEnabled;
}

void GAdvancedWidget::setStickEnabled(bool val)
{
	Private::stickEnabled = val;
}

void GAdvancedWidget::moveEvent(QMoveEvent *e)
{
	d->moveEvent(e);
}

#include "advwidget.moc"
