/*
 * tabdlg.cpp - dialog for handling tabbed chats
 * Copyright (C) 2005  Kevin Smith
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

#include "tabdlg.h"

#include "iconwidget.h"
#include "iconset.h"
#include "common.h"
#include "psicon.h"
#include <qmenubar.h>
#include <qcursor.h>
#include <q3dragobject.h>
#include <QVBoxLayout>
#include <QDragMoveEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <Q3PopupMenu>
#include <QDropEvent>
#include <QCloseEvent>
#include "psitabwidget.h"
#include "psioptions.h"
#include "shortcutmanager.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

//----------------------------------------------------------------------------
// TabDlg
//----------------------------------------------------------------------------
TabDlg::TabDlg(PsiCon *psiCon)
{
  	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);
	psi=psiCon;

	tabMenu = new Q3PopupMenu( this );
	connect( tabMenu, SIGNAL( aboutToShow() ), SLOT( buildTabMenu() ) );
	
	tabs = new PsiTabWidget (this);

	closeCross = new QPushButton(this);
	//closeCross->setText("x");
	closeCross->setIcon(IconsetFactory::icon("psi/closetab").icon());
	closeCross->hide();
	/*if (option.usePerTabCloseButton)
	  tabs->setHoverCloseButton(true);
	else
	{
		tabs->setHoverCloseButton(false);
		tabs->setCornerWidget( closeCross );
		closeCross->show();
	}*/
	
	//tabs->setCloseIcon(IconsetFactory::icon("psi/closetab").iconSet());
	connect (tabs, SIGNAL( mouseDoubleClickTab( QWidget* ) ), SLOT( detachChat( QWidget* ) ) );
	//connect (tabs, SIGNAL( testCanDecode(const QDragMoveEvent*, bool&) ), SLOT( tabTestCanDecode(const QDragMoveEvent*, bool&) ) );
	//connect (tabs, SIGNAL( receivedDropEvent( QDropEvent* ) ), SLOT( tabReceivedDropEvent( QDropEvent* ) ) );
	//connect (tabs, SIGNAL( receivedDropEvent( QWidget*, QDropEvent* ) ), SLOT( tabReceivedDropEvent( QWidget*, QDropEvent* ) ) );
	//connect (tabs, SIGNAL( initiateDrag( QWidget* ) ), SLOT( startDrag( QWidget* ) ) );
	//connect (tabs, SIGNAL( closeRequest( QWidget* ) ), SLOT( closeChat( QWidget* ) ) );
	
		
	QVBoxLayout *vert1 = new QVBoxLayout( this, 1);
	vert1->addWidget(tabs);
	chats.setAutoDelete( FALSE );
	X11WM_CLASS("chat");
	
	connect( closeCross, SIGNAL( clicked() ), SLOT( closeChat() ) );
	connect( tabs, SIGNAL( currentChanged( QWidget* ) ), SLOT( tabSelected( QWidget* ) ) );	

	setAcceptDrops(TRUE);

	setLooks();

	resize(option.sizeTabDlg);

	act_close = new QAction(this);
	addAction(act_close);
	connect(act_close,SIGNAL(activated()), SLOT(closeChat()));
	act_prev = new QAction(this);
	addAction(act_prev);
	connect(act_prev,SIGNAL(activated()), SLOT(previousTab()));
	act_next = new QAction(this);
	addAction(act_next);
	connect(act_next,SIGNAL(activated()), SLOT(nextTab()));

	setShortcuts();
}

TabDlg::~TabDlg()
{

}

void TabDlg::setShortcuts()
{
	//act_close->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	act_prev->setShortcuts(ShortcutManager::instance()->shortcuts("chat.previous-tab"));
	act_next->setShortcuts(ShortcutManager::instance()->shortcuts("chat.next-tab"));
}

void TabDlg::resizeEvent(QResizeEvent *e)
{
  if(option.keepSizes)
	option.sizeTabDlg = e->size();
}

void TabDlg::buildTabMenu()
{
	tabMenu->clear();
	tabMenu->insertItem( tr("Detach Current Tab"), this, SLOT( detachChat() ) );
	tabMenu->insertItem( tr("Close Current Tab"), this, SLOT( closeChat() ) );

	Q3PopupMenu* sendTo = new Q3PopupMenu(tabMenu);
	for (uint i = 0; i < psi->getTabSets()->count(); ++i)
	{
		TabDlg* tabSet= psi->getTabSets()->at(i);
		sendTo->insertItem( tabSet->getName(), this, SLOT( sendChatTo( tabSet ) ) );
	}
	tabMenu->insertItem( tr("Sent Current Tab to"), sendTo);
}

void TabDlg::sendChatTo(QWidget* chatw, TabDlg* otherTabs)
{
	if (otherTabs==this)
		return;
	ChatDlg* chat = (ChatDlg*)chatw;
	closeChat(chat, false);
	otherTabs->addChat(chat);
}

void TabDlg::optionsUpdate()
{
	setShortcuts();
}

void TabDlg::setLooks()
{
	//set the widget icon
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/start-chat").icon());
#endif
	tabs->setTabPosition(QTabWidget::Top);
	if (option.putTabsAtBottom)
		tabs->setTabPosition(QTabWidget::Bottom);

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);
}

QString TabDlg::getName()
{
	return ((ChatDlg*)(tabs->currentPage()))->getDisplayNick();
}

void TabDlg::tabSelected(QWidget* chat)
{
	((ChatDlg*)chat)->activated(); //is this still necessary?
	updateCaption();
}

bool TabDlg::managesChat(ChatDlg* chat)
{
	if ( chats.contains(chat) )
			return true;
	return false;
}

bool TabDlg::chatOnTop(ChatDlg* chat)
{
	if ( tabs->currentPage() == chat )
		return true;
	return false;
}

void TabDlg::addChat(ChatDlg* chat)
{
	chats.append(chat);
	tabs->addTab(chat, chat->getDisplayNick());
	tabs->setTabIconSet(chat, IconsetFactory::icon("psi/start-chat").icon());

	tabs->showPage(chat);
	connect ( chat, SIGNAL( captionChanged( ChatDlg*) ), SLOT( updateTab( ChatDlg* ) ) );
	connect ( chat, SIGNAL( contactStateChanged( XMPP::ChatState ) ), SLOT( setTabState( XMPP::ChatState ) ) );
	connect ( chat, SIGNAL( unreadMessageUpdate(ChatDlg*, int) ), SLOT( setTabHasMessages(ChatDlg*, int) ) );
	
	this->show();
	updateCaption();
}

void TabDlg::detachChat()
{
	detachChat(tabs->currentPage());
}

void TabDlg::detachChat(QWidget* chat)
{
	//don't detach singleton chats, fix for flyspray #477
	if (tabs->count()==1)
		return;
	
	TabDlg *newTab = psi->newTabs();
	sendChatTo(chat, newTab);
}

void TabDlg::closeChat()
{
	ChatDlg* chat = (ChatDlg*)(tabs->currentPage());
	closeChat(chat);
}

/**
 * Removes the chat from the tabset, 'closing' it if specified.
 * The method is used without closing tabs when transferring from one
 * tabset to another.
 * \param chat Chat to remove.
 * \param doclose Whether the chat is 'closed' while removing it.
 */ 
void TabDlg::closeChat(ChatDlg* chat, bool doclose=true)
{
	if (doclose && !chat->readyToHide()) {
		return;
	}
	chat->hide();
	disconnect ( chat, SIGNAL( captionChanged( ChatDlg*) ), this, SLOT( updateTab( ChatDlg* ) ) );
	disconnect ( chat, SIGNAL( contactStateChanged( XMPP::ChatState ) ), this, SLOT( setTabState( XMPP::ChatState ) ) );
	disconnect ( chat, SIGNAL( unreadMessageUpdate(ChatDlg*, int) ), this, SLOT( setTabHasMessages(ChatDlg*, int) ) );
	tabs->removePage(chat);
	tabIsComposing.erase(chat);
	tabHasMessages.erase(chat);
	chats.remove(chat);
	chat->reparent(0,QPoint());
	//if (doclose)
	//	chat->close();
	if (tabs->count()>0)
		updateCaption();
	checkHasChats();
}

void TabDlg::closeChat(QWidget* chat)
{
	closeChat((ChatDlg*)chat);
}

void TabDlg::selectTab(ChatDlg* chat)
{
	tabs->showPage(chat);
}

void TabDlg::checkHasChats()
{
	if (tabs->count()>0)
		return;
	closeMe();
}

void TabDlg::windowActivationChange(bool oldstate)
{
	QWidget::windowActivationChange(oldstate);

	// if we're bringing it to the front, get rid of the '*' if necessary
	if( isActiveWindow() ) { 
		activated();
	}
}

void TabDlg::activated()
{
	updateCaption();
	doFlash(false);
}

void TabDlg::updateCaption()
{
	QString cap = "";
	uint pending=0;
	for ( int i=0; i<tabHasMessages.count(); ++i)
	{
		pending+=tabHasMessages.values()[i];
	}
	if(pending > 0) {
		cap += "* ";
		if(pending > 1)
			cap += QString("[%1] ").arg(pending);
	}
	cap += getName();
	if (tabIsComposing[(ChatDlg*)(tabs->currentPage())])
		cap += tr(" is composing");
	setWindowTitle(cap);
}

void TabDlg::closeEvent(QCloseEvent* closeEvent)
{
	Q_UNUSED(closeEvent);
	int count=tabs->count();
	for (int i=0;i<count;++i) {
		closeChat();
	}
}

void TabDlg::closeMe()
{
	emit isDying(this);
	//we do not delete it here, let the PsiCon do that, they create, they destroy.
}

ChatDlg* TabDlg::getChatPointer(QString fullJid)
{
	for (int i=0; i < tabs->count() ; i++)
	{
		if (((ChatDlg*)tabs->page(i))->jid().full()==fullJid)
		{
			return (ChatDlg*)(tabs->page(i));
		}
	}
	return false;
}

void TabDlg::updateTab( ChatDlg* chat)
{
	QString label, prefix;
	int num=tabHasMessages[chat];
	if (num == 0)
	{
		prefix="";
	} 
	else if (num == 1) 
	{
		prefix="* ";
	}
	else
	{
		prefix=QString("[%1] ").arg(num);
	}

	label=prefix+chat->getDisplayNick();
	tabs->setTabLabel( chat, label );
	//now set text colour based upon whether there are new messages/composing etc

	if (tabIsComposing[chat])
		tabs->setTabTextColor( chat, Qt::darkGreen );
	else if (tabHasMessages[chat])
	{
		tabs->setTabTextColor( chat, Qt::red );
		if (PsiOptions::instance()->getOption("options.ui.flash-windows").toBool())
			doFlash(true);
	}
	else
		tabs->setTabTextColor( chat, Qt::black );
	updateCaption();
}

void TabDlg::setTabState( XMPP::ChatState state )
{
	ChatDlg* chat = (ChatDlg*) sender();
	if ( state == XMPP::StateComposing )
		tabIsComposing[chat] = true;
	else
		tabIsComposing[chat] = false;
	updateTab(chat);
}

void TabDlg::setTabHasMessages(ChatDlg* chat, int messages)
{
	tabHasMessages[chat]=messages;
	updateTab(chat);
}

void TabDlg::nextTab()
{
	int page = tabs->currentPageIndex()+1;
	if ( page >= tabs->count() )
		page = 0;
	tabs->setCurrentPage( page );
}

void TabDlg::previousTab()
{
	int page = tabs->currentPageIndex()-1;
	if ( page < 0 )
		page = tabs->count() - 1;
	tabs->setCurrentPage( page );
}

void TabDlg::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Escape)
	{
		closeChat();
	}
	else if ( e->key() == Qt::Key_W && (e->modifiers() & Qt::ControlModifier) )
	{
		closeChat();
	}
	else if ( e->key() == Qt::Key_PageUp && (e->modifiers() & Qt::ControlModifier) )
	{
		previousTab();
	}
	else if ( e->key() == Qt::Key_PageDown && (e->modifiers() & Qt::ControlModifier) )
	{
		nextTab();
	}
	else
		e->ignore();
	
}

void TabDlg::dragEnterEvent(QDragEnterEvent *event)
{
	if ( event->mimeData()->hasFormat("psiTabDrag") ) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
	}
}

void TabDlg::dropEvent(QDropEvent *event)
{
	QByteArray data;
	if (event->mimeData()->hasFormat("psiTabDrag")) {
		data = event->mimeData()->data("psiTabDrag");
	} else {
		return;
	}
	int remoteTab = data.toInt();
	event->acceptProposedAction();
	//the event's been and gone, now do something about it
	PsiTabBar* source = dynamic_cast<PsiTabBar*> (event->source());
	if (source)
	{
		PsiTabWidget* barParent = source->psiTabWidget();
		QWidget* widget = barParent->widget(remoteTab);
		ChatDlg* chat=dynamic_cast<ChatDlg*>(widget);
		TabDlg *dlg=psi->getManagingTabs(chat);
		if (!chat || !dlg)
			return;
		qRegisterMetaType<TabDlg*>("TabDlg*");
		QMetaObject::invokeMethod(dlg, "sendChatTo",  Qt::QueuedConnection, Q_ARG(QWidget*, chat), Q_ARG(TabDlg*, this));
		//dlg->sendChatTo(chat, this);
	} 
	
}
