/*
 * psitabwidget.h - Customised QTabWidget for Psi
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


#ifndef PSITABWIDGET_H
#define PSITABWIDGET_H

#include <QTabWidget>
#include <QTabBar>
#include <QDragEnterEvent>
#include "psitabbar.h"

class QVBoxLayout;
class QHBoxLayout;
class QToolButton;
class QStackedLayout;
class QMenu;

/**
 * \class PsiTabWidget
 * \brief 
 */
class PsiTabWidget : public QWidget //: public QTabWidget
{
	Q_OBJECT
public:
	PsiTabWidget(QWidget *parent = 0);
	~PsiTabWidget();
	
	void setTabTextColor( QWidget* tab, const QColor& color);
	int count();
	QWidget* currentPage();
	int currentPageIndex();
	QWidget* widget(int index);
	void addTab(QWidget*, QString);
	void showPage(QWidget*);
	void showPageDirectly(QWidget*);
	
	void removePage(QWidget*);
	QWidget* page(int index);
	int getIndex(QWidget*);
	void setTabLabel(QWidget*, const QString&);
	void setTabPosition(QTabWidget::TabPosition pos);
	void setCloseIcon(const QIcon&);

public slots:
	void setCurrentPage(int);
	void removeCurrentPage();

signals:
	void mouseDoubleClickTab( QWidget* tab );
	void currentChanged(QWidget*);
	void closeButtonClicked();
	void aboutToShowMenu(QMenu *);
	// context menu on the blank space will have tab==-1
	void tabContextMenu( int tab, QPoint pos, QContextMenuEvent * event);


private slots:
	void mouseDoubleClickTab( int tab );
	void tab_currentChanged( int tab );
	void tab_contextMenu( QContextMenuEvent * event, int tab);
	void menu_aboutToShow();
	void menu_triggered(QAction *act);
	
private:
	QVector<QWidget*> widgets_;
	QTabBar* tabBar_;
	QVBoxLayout* layout_;
	QHBoxLayout *barLayout_;
	QStackedLayout *stacked_;
	QToolButton *closeButton_;
	QToolButton *downButton_;
	QTabWidget::TabPosition tabsPosition_;
	QMenu *menu_;
}; 


#endif
