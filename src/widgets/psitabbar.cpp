/* Copyright (C) 2005 Kevin Smith
		notice goes here
	 */


/* This file is heavily based on part of the KDE libraries
    Copyright (C) 2003 Stephan Binner <binner@kde.org>
    Copyright (C) 2003 Zack Rusin <zack@kde.org>
*/

#include <qapplication.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qstyle.h>
#include <qtimer.h>
#include <qpushbutton.h>

#include "psitabbar.h"
#include "psitabwidget.h"

KTabBar::KTabBar( QWidget *parent, const char *name )
    : QTabBar( parent, name ), mReorderStartTab( -1 ), mReorderPreviousTab( -1 ),
      mHoverCloseButtonTab( 0 ), mDragSwitchTab( 0 ), mHoverCloseButton( 0 ),
      mHoverCloseButtonEnabled( false ), mHoverCloseButtonDelayed( true ),
      mTabReorderingEnabled( false ), mTabCloseActivatePrevious( false )
{
    setAcceptDrops( true );
    setMouseTracking( true );

    mEnableCloseButtonTimer = new QTimer( this );
    connect( mEnableCloseButtonTimer, SIGNAL( timeout() ), SLOT( enableCloseButton() ) );

    mActivateDragSwitchTabTimer = new QTimer( this );
    connect( mActivateDragSwitchTabTimer, SIGNAL( timeout() ), SLOT( activateDragSwitchTab() ) );

    connect(this, SIGNAL(layoutChanged()), SLOT(onLayoutChange()));
		dndEventDelay=5;
}

KTabBar::~KTabBar()
{
    //For the future
    //delete d;
}

void KTabBar::setCloseIcon(const QIconSet& icon)
{
	closeIcon=icon;
}

void KTabBar::setTabEnabled( int id, bool enabled )
{
    QTab * t = tab( id );
    if ( t ) {
        if ( t->isEnabled() != enabled ) {
            t->setEnabled( enabled );
            QRect r( t->rect() );
            if ( !enabled && id == currentTab() && count()>1 ) {
                QPtrList<QTab> *tablist = tabList();
                if ( mTabCloseActivatePrevious )
                    t = tablist->at( count()-2 );
                else {
                int index = indexOf( id );
                index += ( index+1 == count() ) ? -1 : 1;
                t = tabAt( index );
                }

                if ( t->isEnabled() ) {
                    r = r.unite( t->rect() );
                    tablist->append( tablist->take( tablist->findRef( t ) ) );
                    emit selected( t->identifier() );
                }
            }
            repaint( r );
        }
    }
}

void KTabBar::mouseDoubleClickEvent( QMouseEvent *e )
{
    if( e->button() != LeftButton )
        return;

    QTab *tab = selectTab( e->pos() );
    if( tab ) {
        emit( mouseDoubleClick( indexOf( tab->identifier() ) ) );
        return;
    }
    QTabBar::mouseDoubleClickEvent( e );
}

void KTabBar::mousePressEvent( QMouseEvent *e )
{
    if( e->button() == LeftButton ) {
        mEnableCloseButtonTimer->stop();
        mDragStart = e->pos();
    }
    else if( e->button() == RightButton ) {
        QTab *tab = selectTab( e->pos() );
        if( tab ) {
            emit( contextMenu( indexOf( tab->identifier() ), mapToGlobal( e->pos() ) ) );
            return;
        }
    }
    QTabBar::mousePressEvent( e );
}

void KTabBar::mouseMoveEvent( QMouseEvent *e )
{
    if ( e->state() == LeftButton ) {
        QTab *tab = selectTab( e->pos() );
        if ( mDragSwitchTab && tab != mDragSwitchTab ) {
          mActivateDragSwitchTabTimer->stop();
          mDragSwitchTab = 0;
        }

        QPoint newPos = e->pos();
        if( newPos.x() > mDragStart.x()+dndEventDelay || newPos.x() < mDragStart.x()-dndEventDelay ||
            newPos.y() > mDragStart.y()+dndEventDelay || newPos.y() < mDragStart.y()-dndEventDelay )
         {
            if( tab ) {
                emit( initiateDrag( indexOf( tab->identifier() ) ) );
                return;
           }
       }
    }
    else if ( e->state() == MidButton ) {
        if (mReorderStartTab==-1) {
            QPoint newPos = e->pos();
            if( newPos.x() > mDragStart.x()+dndEventDelay || newPos.x() < mDragStart.x()-dndEventDelay ||
                newPos.y() > mDragStart.y()+dndEventDelay || newPos.y() < mDragStart.y()-dndEventDelay )
            {
                QTab *tab = selectTab( e->pos() );
                if( tab && mTabReorderingEnabled ) {
                    mReorderStartTab = indexOf( tab->identifier() );
                    grabMouse( sizeAllCursor );
                    return;
                }
            }
        }
        else {
            QTab *tab = selectTab( e->pos() );
            if( tab ) {
                int reorderStopTab = indexOf( tab->identifier() );
                if ( mReorderStartTab!=reorderStopTab && mReorderPreviousTab!=reorderStopTab ) {
                    emit( moveTab( mReorderStartTab, reorderStopTab ) );
                    mReorderPreviousTab=mReorderStartTab;
                    mReorderStartTab=reorderStopTab;
                    return;
                }
            }
        }
    }

    if ( mHoverCloseButtonEnabled && mReorderStartTab==-1) {
        QTab *t = selectTab( e->pos() );
        if( t && t->iconSet() && t->isEnabled() ) {
            QPixmap pixmap = t->iconSet()->pixmap( QIconSet::Small, QIconSet::Normal );
            QRect rect( 0, 0, pixmap.width() + 4, pixmap.height() +4);

            int xoff = 0, yoff = 0;
            // The additional offsets were found by try and error, TODO: find the rational behind them
            if ( t == tab( currentTab() ) ) {
                xoff = style().pixelMetric( QStyle::PM_TabBarTabShiftHorizontal, this ) + 3;
                yoff = style().pixelMetric( QStyle::PM_TabBarTabShiftVertical, this ) - 4;
            }
            else {
                xoff = 7;
                yoff = 0;
            }
            rect.moveLeft( t->rect().left() + 2 + xoff );
            rect.moveTop( t->rect().center().y()-pixmap.height()/2 + yoff );
            if ( rect.contains( e->pos() ) ) {
                if ( mHoverCloseButton ) {
                    if ( mHoverCloseButtonTab == t )
                        return;
                    mEnableCloseButtonTimer->stop();
                    delete mHoverCloseButton;
                }

                mHoverCloseButton = new QPushButton( this );
                mHoverCloseButton->setIconSet( closeIcon);
                mHoverCloseButton->setGeometry( rect );
                mHoverCloseButton->setToolTip(tr("Close this tab"));
                mHoverCloseButton->setFlat(true);
                mHoverCloseButton->show();
                if ( mHoverCloseButtonDelayed ) {
                  mHoverCloseButton->setEnabled(false);
                  mEnableCloseButtonTimer->start( QApplication::doubleClickInterval(), true );
                }
                mHoverCloseButtonTab = t;
                connect( mHoverCloseButton, SIGNAL( clicked() ), SLOT( closeButtonClicked() ) );
                return;
            }
        }
        if ( mHoverCloseButton ) {
            mEnableCloseButtonTimer->stop();
            delete mHoverCloseButton;
            mHoverCloseButton = 0;
        }
    }

    QTabBar::mouseMoveEvent( e );
}

void KTabBar::enableCloseButton()
{
    mHoverCloseButton->setEnabled(true);
}

void KTabBar::activateDragSwitchTab()
{
    QTab *tab = selectTab( mapFromGlobal( QCursor::pos() ) );
    if ( tab && mDragSwitchTab == tab )
    setCurrentTab( mDragSwitchTab );
    mDragSwitchTab = 0;
}

void KTabBar::mouseReleaseEvent( QMouseEvent *e )
{
    if( e->button() == MidButton ) {
        if ( mReorderStartTab==-1 ) {
            QTab *tab = selectTab( e->pos() );
            if( tab ) {
                emit( mouseMiddleClick( indexOf( tab->identifier() ) ) );
                return;
            }
        }
        else {
            releaseMouse();
            setCursor( arrowCursor );
            mReorderStartTab=-1;
            mReorderPreviousTab=-1;
        }
    }
    QTabBar::mouseReleaseEvent( e );
}

void KTabBar::dragMoveEvent( QDragMoveEvent *e )
{
    QTab *tab = selectTab( e->pos() );
    if( tab ) {
        bool accept = false;
        // The receivers of the testCanDecode() signal has to adjust
        // 'accept' accordingly.
        emit testCanDecode( e, accept);
        if ( accept && tab != QTabBar::tab( currentTab() ) ) {
          mDragSwitchTab = tab;
          mActivateDragSwitchTabTimer->start( QApplication::doubleClickInterval()*2, true );
        }
        e->accept( accept );
        return;
    }
    e->accept( false );
    QTabBar::dragMoveEvent( e );
}

void KTabBar::dropEvent( QDropEvent *e )
{
    QTab *tab = selectTab( e->pos() );
    if( tab ) {
        mActivateDragSwitchTabTimer->stop();
        mDragSwitchTab = 0;
        emit( receivedDropEvent( indexOf( tab->identifier() ) , e ) );
        return;
    }
    QTabBar::dropEvent( e );
}

#ifndef QT_NO_WHEELEVENT
void KTabBar::wheelEvent( QWheelEvent *e )
{
    if ( e->orientation() == Horizontal )
        return;

    emit( wheelDelta( e->delta() ) );
}
#endif

void KTabBar::setTabColor( int id, const QColor& color )
{
    QTab *t = tab( id );
    if ( t ) {
        mTabColors.insert( id, color );
        repaint( t->rect(), false );
    }
}

const QColor &KTabBar::tabColor( int id  ) const
{
    if ( mTabColors.contains( id ) )
        return mTabColors[id];

    return colorGroup().foreground();
}

int KTabBar::insertTab( QTab *t, int index )
{
    int res = QTabBar::insertTab( t, index );

    if ( mTabCloseActivatePrevious && count() > 2 ) {
        QPtrList<QTab> *tablist = tabList();
        tablist->insert( count()-2, tablist->take( tablist->findRef( t ) ) );
    }

    return res;
}

void KTabBar::removeTab( QTab *t )
{
    mTabColors.remove( t->identifier() );
    QTabBar::removeTab( t );
}

void KTabBar::paintLabel( QPainter *p, const QRect& br,
                          QTab *t, bool has_focus ) const
{
    QRect r = br;
    bool selected = currentTab() == t->identifier();
    if ( t->iconSet() ) {
        // the tab has an iconset, draw it in the right mode
        QIconSet::Mode mode = ( t->isEnabled() && isEnabled() )
                                 ? QIconSet::Normal : QIconSet::Disabled;
        if ( mode == QIconSet::Normal && has_focus )
            mode = QIconSet::Active;
        QPixmap pixmap = t->iconSet()->pixmap( QIconSet::Small, mode );
        int pixw = pixmap.width();
        int pixh = pixmap.height();
        r.setLeft( r.left() + pixw + 4 );
        r.setRight( r.right() + 2 );

        int inactiveXShift = style().pixelMetric( QStyle::PM_TabBarTabShiftHorizontal, this );
        int inactiveYShift = style().pixelMetric( QStyle::PM_TabBarTabShiftVertical, this );

        int right = t->text().isEmpty() ? br.right() - pixw : br.left() + 2;

        p->drawPixmap( right + (selected ? 0 : inactiveXShift),
                       br.center().y() - pixh / 2 + (selected ? 0 : inactiveYShift),
                       pixmap );
    }

    QStyle::SFlags flags = QStyle::Style_Default;

    if ( isEnabled() && t->isEnabled() )
        flags |= QStyle::Style_Enabled;
    if ( has_focus )
        flags |= QStyle::Style_HasFocus;

    QColorGroup cg( colorGroup() );
    if ( mTabColors.contains( t->identifier() ) )
        cg.setColor( QColorGroup::Foreground, mTabColors[t->identifier()] );

    style().drawControl( QStyle::CE_TabBarLabel, p, this, r,
                             t->isEnabled() ? cg : palette().disabled(),
                             flags, QStyleOption(t) );
}

bool KTabBar::isTabReorderingEnabled() const
{
    return mTabReorderingEnabled;
}

void KTabBar::setTabReorderingEnabled( bool on )
{
    mTabReorderingEnabled = on;
}

bool KTabBar::tabCloseActivatePrevious() const
{
    return mTabCloseActivatePrevious;
}

void KTabBar::setTabCloseActivatePrevious( bool on )
{
    mTabCloseActivatePrevious = on;
}

void KTabBar::closeButtonClicked()
{
    emit closeRequest( indexOf( mHoverCloseButtonTab->identifier() ) );
}

void KTabBar::setHoverCloseButton( bool button )
{
    mHoverCloseButtonEnabled = button;
    if ( !button )
        onLayoutChange();
}

bool KTabBar::hoverCloseButton() const
{
    return mHoverCloseButtonEnabled;
}

void KTabBar::setHoverCloseButtonDelayed( bool delayed )
{
    mHoverCloseButtonDelayed = delayed;
}

bool KTabBar::hoverCloseButtonDelayed() const
{
    return mHoverCloseButtonDelayed;
}

void KTabBar::onLayoutChange()
{
    mEnableCloseButtonTimer->stop();
    delete mHoverCloseButton;
    mHoverCloseButton = 0;
    mHoverCloseButtonTab = 0;
    mActivateDragSwitchTabTimer->stop();
    mDragSwitchTab = 0;
}

