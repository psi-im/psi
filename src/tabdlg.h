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
#include <Q3PtrList>

#include "xmpp_chatstate.h"
#include "advwidget.h"

#include "tabbable.h"


class PsiCon;
class ChatTabs;
class ChatDlg;
class QPushButton;
class QMenu;
class QString;
class Q3DragObject;
class QContextMenuEvent;
class PsiTabWidget;

class TabDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	TabDlg(PsiCon*);
	~TabDlg();
	bool managesTab(Tabbable*);
	bool tabOnTop(Tabbable*);
	QString getName();
	Tabbable *getTab(int i);
	
signals:
	void isDying(TabDlg*);
protected:
	void setShortcuts();
	void closeEvent( QCloseEvent* );
	void keyPressEvent(QKeyEvent *);
	void windowActivationChange(bool);
	void resizeEvent(QResizeEvent *);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
protected slots:
	void detachChat();
	void detachChat(QWidget*);
	void closeChat();
	void closeChat(QWidget*);
	void sendChatTo(QWidget*, TabDlg *);
	void queuedSendChatTo(QWidget*, TabDlg *);
public slots:
	void addTab(Tabbable *tab);
	void setLooks();
	void closeTab(Tabbable*,bool);
	void selectTab(Tabbable*);
	void activated();
	void optionsUpdate();
private slots:
	void tabSelected(QWidget* chat);
	void checkHasChats();
	void closeMe();
	void updateTab(QString);
	void updateTab(Tabbable*);
	void nextTab();
	void previousTab();
	void setTabState( XMPP::ChatState );
	void setTabHasEvents(int);
	void tab_aboutToShowMenu(QMenu *menu);
	void menu_sendChatTo(QAction *act);
	void showTabMenu(int tab, QPoint pos, QContextMenuEvent * event);

	
public:
	Tabbable* getTabPointer(QString fullJid);
private:
	void updateCaption();
	PsiCon *psi;
	Q3PtrList<Tabbable> chats;
	PsiTabWidget *tabs;
	QPushButton *detachButton, *closeButton, *closeCross;
	QMenu *tabMenu;
	QMap<Tabbable*, bool> tabIsComposing;
	QMap<Tabbable*, int> tabHasMessages;
	QAction *act_close, *act_next, *act_prev;

	QSize chatSize;
};

#endif
