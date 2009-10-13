/*
 * tabdlg.h - dialog for handling tabbed chats
 * Copyright (C) 2005 Kevin Smith
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

#ifndef TABDLG_H
#define TABDLG_H

#include <QWidget>
#include <QSize>
#include <QMap>
#include <QPointer>

#include "advwidget.h"
#include "tabbablewidget.h"
#include "verticaltabbar.h"

class PsiCon;
class ChatTabs;
class ChatDlg;
class QPushButton;
class QMenu;
class QString;
class Q3DragObject;
class QContextMenuEvent;
class PsiTabWidget;
class TabManager;

class TabDlg;

class TabDlgDelegate : public QObject
{
	Q_OBJECT
public:
	TabDlgDelegate(QObject *parent = 0);
	~TabDlgDelegate();

	virtual Qt::WindowFlags initWindowFlags() const;
	virtual void create(QWidget *widget);
	virtual void destroy(QWidget *widget);
	virtual void tabWidgetCreated(QWidget *widget, PsiTabWidget *tabWidget);
	virtual bool paintEvent(QWidget *widget, QPaintEvent *event);
	virtual bool resizeEvent(QWidget *widget, QResizeEvent *event);
	virtual bool mousePressEvent(QWidget *widget, QMouseEvent *event);
	virtual bool mouseMoveEvent(QWidget *widget, QMouseEvent *event);
	virtual bool mouseReleaseEvent(QWidget *widget, QMouseEvent *event);
	virtual bool changeEvent(QWidget *widget, QEvent *event);
	virtual bool event(QWidget *widget, QEvent *event);
	virtual bool eventFilter(QWidget *widget, QObject *obj, QEvent *event);
};

class TabDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	TabDlg(TabManager* tabManager, QSize size, TabDlgDelegate *delegate = 0);
	~TabDlg();
	bool managesTab(const TabbableWidget*) const;
	bool tabOnTop(const TabbableWidget*) const;
	TabbableWidget *getTab(int i) const;
	void removeTabWithNoChecks(TabbableWidget *tab);

	TabbableWidget* getTabPointer(QString fullJid);

	virtual QString desiredCaption() const;
	QString captionForTab(TabbableWidget* tab) const;

	int tabCount() const;
	void setUserManagementEnabled(bool enabled); // default enabled
	void setTabBarShownForSingles(bool enabled); // default enabled
	void setSimplifiedCaptionEnabled(bool enabled); // default disabled
	void setVerticalTabsEnabled(bool enabled); // default disabled

protected:
	void setShortcuts();

	// reimplemented
	void closeEvent(QCloseEvent*);
	void changeEvent(QEvent *event);
	void resizeEvent(QResizeEvent *);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);

	// delegate-only
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual bool event(QEvent *event);
	virtual bool eventFilter(QObject *obj, QEvent *event);

protected slots:
	void detachCurrentTab();
	void mouseDoubleClickTab(QWidget*);

public slots:
	void addTab(TabbableWidget *tab);
	void setLooks();
	void closeCurrentTab();
	void closeTab(TabbableWidget*, bool doclose = true);
	void selectTab(TabbableWidget*);
	void activated();
	void optionsUpdate();
	void detachTab(TabbableWidget*);
	void sendTabTo(TabbableWidget*, TabDlg *);

signals:
	void resized(QSize size);
	
private slots:
	void updateFlashState();
	void tabSelected(QWidget* selected);
	void checkHasChats();
	void updateTab();
	void updateTab(TabbableWidget*);
	void nextTab();
	void previousTab();
	void tab_aboutToShowMenu(QMenu *menu);
	void setAsDefaultForChat();
	void setAsDefaultForMuc();
	void menu_sendTabTo(QAction *act);
	void queuedSendTabTo(TabbableWidget* chat, TabDlg *dest);
	void showTabMenu(int tab, QPoint pos, QContextMenuEvent * event);
	void vt_tabClicked(int num);
	void vt_tabMenuActivated(int num, const QPoint &pos);
	void toggleTabsPosition();

private:
	TabDlgDelegate *delegate_;
	QList<TabbableWidget*> tabs_;
	PsiTabWidget *tabWidget_;
	QPushButton *detachButton_;
	QPushButton *closeButton_;
	QPushButton *closeCross_;
	QMenu *tabMenu_;
	QAction *act_close_;
	QAction *act_next_;
	QAction *act_prev_;
	TabManager *tabManager_;
	QPointer<TabbableWidget> selectedTab_;
	bool userManagement_;
	bool tabBarSingles_;
	bool simplifiedCaption_;
	bool verticalTabs_;
	VerticalTabBar *tabBar_;

	QSize chatSize_;

	void extinguishFlashingTabs();
	void updateCaption();
	void updateTabBar();
};

#endif
