/*
 * psitabbar.cpp - Tabbar child for Psi
 * Copyright (C) 2006  Kevin Smith
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

#include "psitabbar.h"
#include "psitabwidget.h"
#include <QMouseEvent>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QPainter>

#include "psioptions.h"

/**
 * Constructor
 */
PsiTabBar::PsiTabBar(PsiTabWidget *parent)
		: QTabBar(parent)
		, dragsEnabled_(true) {
	//setAcceptDrops(true);

	setMovable(true);
	setTabsClosable(true);
	setSelectionBehaviorOnRemove ( QTabBar::SelectPreviousTab );
	currTab=-1;
}

/**
 * Destructor
 */
PsiTabBar::~PsiTabBar() {
}

/**
 * Returns the parent PsiTabWidget.
 */
PsiTabWidget* PsiTabBar::psiTabWidget() {
	return dynamic_cast<PsiTabWidget*> (parent());
}

/**
 * Overriding this allows us to emit signals for double clicks
 */
void PsiTabBar::mouseDoubleClickEvent(QMouseEvent *event) {
	const QPoint pos = event->pos();
	int tab = findTabUnder(pos);
	if (tab >= 0 && tab < count()) {
		emit mouseDoubleClickTab(tab);
	}
}

/*
 * Returns the index of the tab at a position, or -1 if out of bounds.
 */
int PsiTabBar::findTabUnder(const QPoint &pos) {
	for (int i = 0; i < count(); i++) {
		if (tabRect(i).contains(pos)) {
			return i;
		}
	}
	return -1;
}

void PsiTabBar::mousePressEvent(QMouseEvent *event) {
	QTabBar::mousePressEvent(event);
	event->accept();
}

void PsiTabBar::mouseReleaseEvent ( QMouseEvent * event )
{
	if (event->button() == Qt::MidButton && findTabUnder(event->pos())!=-1) {
		emit mouseMiddleClickTab(findTabUnder(event->pos()));
		event->accept();
	}
	QTabBar::mouseReleaseEvent(event);

	if ((dragTab_ != -1) && (event->button() != Qt::MidButton)) {
		this->setCurrentIndex(currentIndex());
	}
};

/*
 * Used for starting drags of tabs
 */
void PsiTabBar::mouseMoveEvent(QMouseEvent *event) {
	if (!dragsEnabled_) {
		return;
	}
	if (!(event->buttons() & Qt::LeftButton)) {
		currTab=-1;
		return;
	}
	if ((event->pos() - dragStartPosition_).manhattanLength()
		< QApplication::startDragDistance()) {
		return;
	}

	QTabBar::mouseMoveEvent(event);
}

void PsiTabBar::contextMenuEvent(QContextMenuEvent *event) {
	event->accept();
	emit contextMenu(event, findTabUnder(event->pos()));
}

void PsiTabBar::wheelEvent(QWheelEvent *event) {
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;

	int newIndex = currentIndex() - numSteps;

	while (newIndex < 0) {
		newIndex += count();
	}
	newIndex = newIndex % count();

	setCurrentIndex(newIndex);

	event->accept();
}

/*
 * Enable/disable dragging of tabs
 */
void PsiTabBar::setDragsEnabled(bool enabled) {
	dragsEnabled_ = enabled;
}

void PsiTabBar::paintEvent(QPaintEvent *event)
{
	QTabBar::paintEvent(event);
};

void PsiTabBar::resizeEvent(QResizeEvent * event)
{
	QTabBar::resizeEvent(event);
};
