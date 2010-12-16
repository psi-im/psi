#include "qwextend.h"

#ifdef Q_WS_X11

#define protected public
#define private public
#include <QWidget>
#undef protected
#undef private

#include <QCursor>
#include <QObject>
#include <QPixmap>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// taken from qt/x11 (doing this sucks sucks sucks sucks sucks)
void reparent_good(QWidget *that, Qt::WFlags f, bool showIt)
{
    extern void qPRCreate( const QWidget *, Window );
    extern void qt_XDestroyWindow( const QWidget *destroyer, Display *display, Window window );
    extern bool qt_dnd_enable( QWidget* w, bool on );

    QWidget *parent = 0;
    Display *dpy = that->x11Display();
    QCursor oldcurs;
    bool setcurs = that->windowState(Qt::WState_OwnCursor);
    if ( setcurs ) {
	oldcurs = that->cursor();
	that->unsetCursor();
    }

    // dnd unregister (we will register again below)
    bool accept_drops = that->acceptDrops();
    that->setAcceptDrops( false );

    // clear mouse tracking, re-enabled below
    bool mouse_tracking = that->hasMouseTracking();
    that->clearWState(Qt::WState_MouseTracking);

    QWidget* oldtlw = that->window();
    QWidget *oldparent = that->parentWidget();
    WId old_winid = that->winid;
    if ( that->windowFlags(Qt::WType_Desktop) )
	old_winid = 0;
    that->setWinId( 0 );

    // hide and reparent our own window away. Otherwise we might get
    // destroyed when emitting the child remove event below. See QWorkspace.
    XUnmapWindow( that->x11Display(), old_winid );
    XReparentWindow( that->x11Display(), old_winid,
		     RootWindow( that->x11Display(), that->x11Screen() ), 0, 0 );

    if ( that->isTopLevel() ) {
        // input contexts are associated with toplevel widgets, so we need
        // destroy the context here.  if we are reparenting back to toplevel,
        // then we will have another context created, otherwise we will
        // use our new toplevel's context
        that->destroyInputContext();
    }

    if ( that->isTopLevel() || !parent ) // we are toplevel, or reparenting to toplevel
        that->topData()->parentWinId = 0;

    if ( parent != that->parentObj ) {
	if ( that->parentObj )				// remove from parent
	    that->parentObj->removeChild( that );
	if ( parent )				// insert into new parent
	    parent->insertChild( that );
    }
    bool     enable = that->isEnabled();		// remember status
    Qt::FocusPolicy fp = that->focusPolicy();
    QSize    s	    = that->size();
    QPixmap *bgp    = (QPixmap *)that->backgroundPixmap();
    QColor   bgc    = that->bg_col;			// save colors
    QString capt= that->caption();
    that->widget_flags = f;
    that->clearWState( Qt::WState_Created | Qt::WState_Visible | Qt::WState_ForceHide );
    that->create();
    if ( that->isTopLevel() || (!parent || parent->isVisible() ) )
	that->setWState( Qt::WState_ForceHide );	// new widgets do not show up in already visible parents

    const QObjectList *chlist = that->children();
    if ( chlist ) {				// reparent children
	QObjectListIt it( *chlist );
	QObject *obj;
	while ( (obj=it.current()) ) {
	    if ( obj->isWidgetType() ) {
		QWidget *w = (QWidget *)obj;
		if ( !w->isTopLevel() ) {
		    XReparentWindow( that->x11Display(), w->winId(), that->winId(),
				     w->geometry().x(), w->geometry().y() );
		} else if ( w->isPopup()
			    || w->windowFlags(Qt::WStyle_DialogBorder)
			    || w->windowFlags(Qt::WType_Dialog)
			    || w->windowFlags(Qt::WStyle_Tool) ) {
		    XSetTransientForHint( that->x11Display(), w->winId(), that->winId() );
		}
	    }
	    ++it;
	}
    }
    qPRCreate( that, old_winid );
    if ( bgp )
	XSetWindowBackgroundPixmap( dpy, that->winid, bgp->handle() );
    else
	XSetWindowBackground( dpy, that->winid, bgc.pixel(that->x11Screen()) );

    // all this just to do a stupid resize instead of setGeometry
    //setGeometry( p.x(), p.y(), s.width(), s.height() );
    that->resize(s.width(), s.height());

    that->setEnabled( enable );
    that->setFocusPolicy( fp );
    if ( !capt.isNull() ) {
	that->extra->topextra->caption = QString::null;
	that->setWindowTitle( capt );
    }
    if ( showIt )
	that->show();
    if ( old_winid )
	qt_XDestroyWindow( that, dpy, old_winid );
    if ( setcurs )
	that->setCursor(oldcurs);

    that->reparentFocusWidgets( oldtlw );

    // re-register dnd
    if (oldparent)
	oldparent->checkChildrenDnd();

    if ( accept_drops )
	that->setAcceptDrops( true );
    else {
	that->checkChildrenDnd();
	that->topData()->dnd = 0;
	qt_dnd_enable(that, (that->extra && that->extra->children_use_dnd));
    }

    // re-enable mouse tracking
    if (mouse_tracking)
	that->setMouseTracking(mouse_tracking);
}

#endif
