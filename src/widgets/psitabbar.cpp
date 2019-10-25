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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psitabbar.h"

#include "psioptions.h"
#include "psitabwidget.h"

#include <QApplication>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

/**
 * Constructor
 */
PsiTabBar::PsiTabBar(PsiTabWidget *parent) : TabBar(parent)
{
    // setAcceptDrops(true);

    setMovable(true);
    setTabsClosable(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
}

/**
 * Destructor
 */
PsiTabBar::~PsiTabBar() {}

/**
 * Returns the parent PsiTabWidget.
 */
PsiTabWidget *PsiTabBar::psiTabWidget() { return dynamic_cast<PsiTabWidget *>(parent()); }

/**
 * Overriding this allows us to emit signals for double clicks
 */
void PsiTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::MouseButton::LeftButton)
        return;

    const QPoint pos = event->pos();
    int          tab = findTabUnder(pos);
    if (tab >= 0 && tab < count()) {
        emit mouseDoubleClickTab(tab);
    }
}

/*
 * Returns the index of the tab at a position, or -1 if out of bounds.
 */
int PsiTabBar::findTabUnder(const QPoint &pos)
{
    for (int i = 0; i < count(); i++) {
        if (tabRect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

void PsiTabBar::mousePressEvent(QMouseEvent *event)
{
    TabBar::mousePressEvent(event);
    event->accept();
}

void PsiTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton && findTabUnder(event->pos()) != -1) {
        emit mouseMiddleClickTab(findTabUnder(event->pos()));
        event->accept();
    }
    TabBar::mouseReleaseEvent(event);

    if (event->button() != Qt::MidButton) {
        this->setCurrentIndex(currentIndex());
    }
};

void PsiTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    event->accept();
    int tab = findTabUnder(event->pos());
    if (tab < 0)
        tab = currentIndex();

    emit contextMenu(event, tab);
}

void PsiTabBar::wheelEvent(QWheelEvent *event)
{
    if (PsiOptions::instance()->getOption("options.ui.tabs.disable-wheel-scroll").toBool())
        return;

    int numDegrees = event->delta() / 8;
    int numSteps   = numDegrees / 15;

    int newIndex = currentIndex() - numSteps;

    while (newIndex < 0) {
        newIndex += count();
    }
    newIndex = newIndex % count();

    setCurrentIndex(newIndex);

    event->accept();
}

void PsiTabBar::paintEvent(QPaintEvent *event) { TabBar::paintEvent(event); };

void PsiTabBar::resizeEvent(QResizeEvent *event) { QTabBar::resizeEvent(event); };
