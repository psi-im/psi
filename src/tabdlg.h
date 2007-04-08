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

#include <qwidget.h>
#include <q3ptrlist.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <QContextMenuEvent>
#include <QDragMoveEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <Q3PopupMenu>
#include <QDropEvent>
#include <QCloseEvent>
#include <qmap.h>

#include "psitabwidget.h"
#include "im.h" // ChatState
#include "advwidget.h"
#include "chatdlg.h"

class PsiCon;
class ChatTabs;
class Q3DragObject;
class QContextMenuEvent;
class PsiTabWidget;

class TabDlg : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	TabDlg(PsiCon*);
	~TabDlg();
	bool managesChat(ChatDlg*);
	bool chatOnTop(ChatDlg*);
	QString getName();
	
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
	void buildTabMenu();
	void sendChatTo(QWidget*, TabDlg *);
public slots:
	void addChat(ChatDlg *chat);
	void setLooks();
	void closeChat(ChatDlg*,bool);
	void selectTab(ChatDlg*);
	void activated();
	void optionsUpdate();
private slots:
	void tabSelected(QWidget* chat);
	void checkHasChats();
	void closeMe();
	void updateTab(ChatDlg*);
	void nextTab();
	void previousTab();
	void setTabState( XMPP::ChatState );
	void setTabHasMessages(ChatDlg*, int);
	
public:
	ChatDlg* getChatPointer(QString fullJid);
private:
	void updateCaption();
	PsiCon *psi;
	Q3PtrList<ChatDlg> chats;
	PsiTabWidget *tabs;
	QPushButton *detachButton, *closeButton, *closeCross;
	Q3PopupMenu *tabMenu;
	QMap<ChatDlg*, bool> tabIsComposing;
	QMap<ChatDlg*, bool> tabHasMessages;
	QAction *act_close, *act_next, *act_prev;

	QSize chatSize;
};

#endif
