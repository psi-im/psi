/*
 * hoverabletreeview.cpp - QTreeView that allows to show hovered items apart
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "hoverabletreeview.h"

#include <QMouseEvent>
#include <QScrollBar>

#ifdef Q_WS_WIN
#include <QLibrary>
#include <windows.h>

// taken from qapplication_win.cpp
#if (_WIN32_WINNT < 0x0400)
// This struct is defined in winuser.h if the _WIN32_WINNT >= 0x0400 -- in the
// other cases we have to define it on our own.
typedef struct tagTRACKMOUSEEVENT {
    DWORD cbSize;
    DWORD dwFlags;
    HWND  hwndTrack;
    DWORD dwHoverTime;
} TRACKMOUSEEVENT, *LPTRACKMOUSEEVENT;
#endif
#endif

//----------------------------------------------------------------------------
// HoverableStyleOptionViewItem
//----------------------------------------------------------------------------

HoverableStyleOptionViewItem::HoverableStyleOptionViewItem()
	: HoverableStyleOptionViewItemBaseClass(Version)
	, hovered(false)
{
}

HoverableStyleOptionViewItem::HoverableStyleOptionViewItem(const QStyleOptionViewItem& other)
	: HoverableStyleOptionViewItemBaseClass(Version)
{
	(void)HoverableStyleOptionViewItem::operator=(other);
}

HoverableStyleOptionViewItem &HoverableStyleOptionViewItem::operator=(const QStyleOptionViewItem& other)
{
	HoverableStyleOptionViewItemBaseClass::operator=(other);
	const HoverableStyleOptionViewItem* hoverable = qstyleoption_cast<const HoverableStyleOptionViewItem*>(&other);
	this->hovered = hoverable ? hoverable->hovered : false;
	this->hoveredPosition = hoverable ? hoverable->hoveredPosition : QPoint();
	return *this;
}

HoverableStyleOptionViewItem::HoverableStyleOptionViewItem(int version)
	: HoverableStyleOptionViewItemBaseClass(version)
{
}

//----------------------------------------------------------------------------
// HoverableTreeView
//----------------------------------------------------------------------------

HoverableTreeView::HoverableTreeView(QWidget* parent)
	: QTreeView(parent)
{
#if QT_VERSION >= 0x040400
	setAutoScrollMargin(50);
#endif
	setMouseTracking(true);
	viewport()->setMouseTracking(true);
}

void HoverableTreeView::mouseMoveEvent(QMouseEvent* event)
{
	mousePosition_ = event->pos();
	QTreeView::mouseMoveEvent(event);

	// required for accurate hovered sub-regions painting
	viewport()->update();
}

void HoverableTreeView::startDrag(Qt::DropActions supportedActions)
{
	mousePosition_ = QPoint();
	QTreeView::startDrag(supportedActions);
}

void HoverableTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const
{
	HoverableStyleOptionViewItem opt = options;

	QRect r = visualRect(index).adjusted(0, 1, 0, 1);
	r.setLeft(0);

	opt.decorationSize = QSize();
	if (underMouse() &&
	    !mousePosition_.isNull() &&
	    r.contains(mousePosition_) &&
	    !verticalScrollBar()->underMouse() &&
	    !verticalScrollBar()->isSliderDown())
	{
		// we're hacking our way through the QTreeView which casts HoverableStyleOptionViewItem
		// down to HoverableStyleOptionViewItemBaseClass and loses the hovered flag
		opt.displayAlignment = Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter | Qt::AlignJustify;

		// decorationSize will hold the mouse coordinates starting from opt.rect.topLeft()
		QPoint offset = mousePosition_ /*- QPoint(opt.rect.left(), 0)*/;
		opt.decorationSize = QSize(offset.x(), offset.y());

		opt.hovered = true;
		opt.hoveredPosition = QPoint(offset.x(), offset.y());
	}
	QTreeView::drawRow(painter, opt, index);
}

void HoverableTreeView::repairMouseTracking()
{
#ifdef Q_WS_WIN
	// work-around for broken mouse event tracking after context menu gets hidden on Qt 4.3.5

	// copied from qapplication_win.cpp
	typedef BOOL (WINAPI *PtrTrackMouseEvent)(LPTRACKMOUSEEVENT);
	static PtrTrackMouseEvent ptrTrackMouseEvent = 0;
	ptrTrackMouseEvent = (PtrTrackMouseEvent)QLibrary::resolve(QLatin1String("comctl32"), "_TrackMouseEvent");

	if (ptrTrackMouseEvent) {
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = 0x00000002;    // TME_LEAVE
		tme.hwndTrack = viewport()->winId(); // Track on window receiving msgs
		tme.dwHoverTime = (DWORD)-1; // HOVER_DEFAULT
		ptrTrackMouseEvent(&tme);
	}
#endif
}

QPoint HoverableTreeView::mousePosition() const
{
	return mousePosition_;
}
