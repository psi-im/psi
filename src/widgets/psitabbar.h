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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _PSITABBAR_H_
#define _PSITABBAR_H_

#include <QTabBar>
#include <QPoint>

class PsiTabWidget;

class PsiTabBar : public QTabBar
{
	Q_OBJECT

public:
	PsiTabBar(PsiTabWidget *parent);
	~PsiTabBar();
	PsiTabWidget *psiTabWidget();

	void setDragsEnabled(bool enabled); // default enabled

signals:
	void mouseDoubleClickTab(int tab);
	void mouseMiddleClickTab(int tab);
	void tabDropped(int tab, PsiTabBar *source);
	// context menu on the blank space will have tab==-1
	void contextMenu(QContextMenuEvent *event, int tab);

protected:
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	//void dragEnterEvent(QDragEnterEvent *event);
	//void dropEvent(QDropEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);
	void wheelEvent(QWheelEvent *event);
	void paintEvent(QPaintEvent *event);
	void mouseReleaseEvent ( QMouseEvent * event );
	void resizeEvent(QResizeEvent * event);

private:
	int findTabUnder(const QPoint &pos);
	QPoint dragStartPosition_;
	int dragTab_;
	int currTab;
	bool isOnTheLeft;
	bool dragsEnabled_;
};

#endif /* _PSITABBAR_H_ */
