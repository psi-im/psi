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
#include <QMouseEvent>

/**
 * Constructor
 */
PsiTabBar::PsiTabBar(QWidget *parent)
	: QTabBar(parent)
{
	
}

/**
 * Destructor
 */
PsiTabBar::~PsiTabBar()
{
	
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