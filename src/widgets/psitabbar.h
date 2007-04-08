/*
 * psitabbar.h - Tabbar child for Psi
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
#ifndef _PSITABBAR_H_
#define _PSITABBAR_H_

#include <QTabBar>


class PsiTabBar : public QTabBar
{
	Q_OBJECT
public:
	PsiTabBar(QWidget *parent = 0);
	~PsiTabBar();

signals:
	void mouseDoubleClickTab( int tab );

protected:
	void mouseDoubleClickEvent( QMouseEvent* event );

private:
	int findTabUnder(const QPoint& pos);
	
};

#endif /* _PSITABBAR_H_ */