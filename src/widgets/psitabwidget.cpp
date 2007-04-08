/*
 * psitabwidget.cpp - Customised QTabWidget for Psi
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

#include "psitabwidget.h"
#include "psitabbar.h"

/**
 * Constructor
 */
PsiTabWidget::PsiTabWidget(QWidget *parent)
	: QTabWidget(parent)
{
	PsiTabBar* tabBar = new PsiTabBar(this);
	setTabBar( tabBar );
	
	connect( tabBar, SIGNAL(mouseDoubleClickTab(int)), SLOT(mouseDoubleClickTab(int)));
}

/**
 * Destructor
 */
PsiTabWidget::~PsiTabWidget()
{
	
}

/**
 * Set the color of text on a tab.
 * \param tab Widget for the tab to change.
 * \param color Color to set text.
 */
void PsiTabWidget::setTabTextColor( QWidget* tab, const QColor& color)
{
	for (int i=0; i < count(); i++) {
		if ( widget(i) == tab ) {
			tabBar()->setTabTextColor(i,color);
		}
	}
}

void PsiTabWidget::mouseDoubleClickTab( int tab )
{
	emit mouseDoubleClickTab(widget(tab));
}




