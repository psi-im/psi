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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psitabbar.h"
#include "psitabwidget.h"
#include <QMouseEvent>
#include <QApplication>

/**
 * Constructor
 */
PsiTabBar::PsiTabBar(PsiTabWidget *parent)
	: QTabBar(parent)
{
	//acceptDrops();
}

/**
 * Destructor
 */
PsiTabBar::~PsiTabBar()
{
	
}

/**
 * Returns the parent PsiTabWidget.
 */
PsiTabWidget* PsiTabBar::psiTabWidget()
{
	return dynamic_cast<PsiTabWidget*> (parent());
}

/**
 * Overriding this allows us to emit signals for double clicks
 */
void PsiTabBar::mouseDoubleClickEvent( QMouseEvent* event )
{
	const QPoint pos = event->pos();
	int tab = findTabUnder( pos );
	if ( tab >=0 && tab < count() )
		emit mouseDoubleClickTab(tab);
}

/*
 * Returns the index of the tab at a position, or -1 if out of bounds.
 */
int PsiTabBar::findTabUnder( const QPoint& pos )
{
	for (int i = 0; i < count(); i++)
	{
		if ( tabRect(i).contains(pos) )
				return i;
	}
	return -1;
} 

void PsiTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPosition_ = event->pos();
		setCurrentIndex( findTabUnder( event->pos() ) );
 	}
	event->accept();
}

/*
 * Used for starting drags of tabs
 */ 
void PsiTabBar::mouseMoveEvent(QMouseEvent *event)
 {
	if (!(event->buttons() & Qt::LeftButton))
		return;
	if ( (event->pos() - dragStartPosition_).manhattanLength() 
		< QApplication::startDragDistance() )
		return;
	QDrag *drag = new QDrag(this);
	QMimeData *mimeData = new QMimeData;
	QByteArray data;
	QPixmap icon;

	int tab=findTabUnder( event->pos() );
	data.setNum(tab);

	mimeData->setData("psiTabDrag",data);
	drag->setMimeData(mimeData);
	drag->setPixmap(icon);
		
	Qt::DropAction dropAction = drag->start(Qt::MoveAction);
	Q_UNUSED(dropAction);

	event->accept();
 }


void PsiTabBar::contextMenuEvent ( QContextMenuEvent * event )
{
	event->accept();
	emit contextMenu(event, findTabUnder(event->pos()));
}

void PsiTabBar::wheelEvent(QWheelEvent *event)
{
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;
	
	int newIndex = currentIndex() - numSteps;

	while (newIndex < 0) newIndex += count();
	newIndex = newIndex % count();

	setCurrentIndex(newIndex);

	event->accept();	
}


