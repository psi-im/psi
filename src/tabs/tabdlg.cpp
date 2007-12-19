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
#include "chatdlg.h"
#include "tabmanager.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

static const QString psiTabDragMimeType = "application/psi-tab-drag";

//----------------------------------------------------------------------------
// TabDlg
//----------------------------------------------------------------------------

TabDlg::TabDlg(TabManager* tabManager)
	: tabWidget_(0)
	, detachButton_(0)
	, closeButton_(0)
	, closeCross_(0)
	, tabMenu_(new QMenu(this))
	, act_close_(0)
	, act_next_(0)
	, act_prev_(0)
	, tabManager_(tabManager)
{
	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);

	// FIXME
	qRegisterMetaType<TabDlg*>("TabDlg*");
	qRegisterMetaType<TabbableWidget*>("TabbableWidget*");

	tabWidget_ = new PsiTabWidget(this);
	tabWidget_->setCloseIcon(IconsetFactory::icon("psi/closetab").icon());
	connect(tabWidget_, SIGNAL(mouseDoubleClickTab(QWidget*)), SLOT(mouseDoubleClickTab(QWidget*)));
	connect(tabWidget_, SIGNAL(aboutToShowMenu(QMenu*)), SLOT(tab_aboutToShowMenu(QMenu*)));
	connect(tabWidget_, SIGNAL(tabContextMenu(int, QPoint, QContextMenuEvent*)), SLOT(showTabMenu(int, QPoint, QContextMenuEvent*)));
	connect(tabWidget_, SIGNAL(closeButtonClicked()), SLOT(closeCurrentTab()));
	connect(tabWidget_, SIGNAL(currentChanged(QWidget*)), SLOT(tabSelected(QWidget*)));

	QVBoxLayout *vert1 = new QVBoxLayout( this, 1);
	vert1->addWidget(tabWidget_);

	setAcceptDrops(TRUE);

	X11WM_CLASS("tabs");
	setLooks();

	act_close_ = new QAction(this);
	addAction(act_close_);
	connect(act_close_,SIGNAL(activated()), SLOT(closeCurrentTab()));
	act_prev_ = new QAction(this);
	addAction(act_prev_);
	connect(act_prev_,SIGNAL(activated()), SLOT(previousTab()));
	act_next_ = new QAction(this);
	addAction(act_next_);
	connect(act_next_,SIGNAL(activated()), SLOT(nextTab()));

	setShortcuts();

	resize(option.sizeTabDlg);
}

TabDlg::~TabDlg()
{
}

// FIXME: This is a bad idea to store pointers in QMimeData
Q_DECLARE_METATYPE(TabDlg*);
Q_DECLARE_METATYPE(TabbableWidget*);

void TabDlg::setShortcuts()
{
	//act_close_->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	act_prev_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.previous-tab"));
	act_next_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.next-tab"));
}

void TabDlg::resizeEvent(QResizeEvent *e)
{
	if (option.keepSizes)
		option.sizeTabDlg = e->size();
}

void TabDlg::showTabMenu(int tab, QPoint pos, QContextMenuEvent * event)
{
	Q_UNUSED(event);
	tabMenu_->clear();

	if (tab != -1) {
		QAction *d = tabMenu_->addAction(tr("Detach Tab"));
		QAction *c = tabMenu_->addAction(tr("Close Tab"));

		QMenu* sendTo = new QMenu(tabMenu_);
		sendTo->setTitle(tr("Send Tab to"));
		QMap<QAction*, TabDlg*> sentTos;
		foreach(TabDlg* tabSet, tabManager_->tabSets()) {
			QAction *act = sendTo->addAction(tabSet->desiredCaption());
			if (tabSet == this)
				act->setEnabled(false);
			sentTos[act] = tabSet;
		}
		tabMenu_->addMenu(sendTo);

		QAction *act = tabMenu_->exec(pos);
		if (!act)
			return;
		if (act == c) {
			closeTab(getTab(tab));
		}
		else if (act == d) {
			detachTab(getTab(tab));
		}
		else {
			TabDlg* target = sentTos[act];
			if (target)
				queuedSendTabTo(getTab(tab), target);
		}
	}
}

void TabDlg::tab_aboutToShowMenu(QMenu *menu)
{
	menu->addSeparator();
	menu->addAction(tr("Detach Current Tab"), this, SLOT(detachCurrentTab()));
	menu->addAction(tr("Close Current Tab"), this, SLOT(closeCurrentTab()));

	QMenu* sendTo = new QMenu(menu);
	sendTo->setTitle(tr("Send Current Tab to"));
	int tabDlgMetaType = qRegisterMetaType<TabDlg*>("TabDlg*");
	foreach(TabDlg* tabSet, tabManager_->tabSets()) {
		QAction *act = sendTo->addAction(tabSet->desiredCaption());
		act->setData(QVariant(tabDlgMetaType, &tabSet));
		act->setEnabled(tabSet != this);
	}
	connect(sendTo, SIGNAL(triggered(QAction*)), SLOT(menu_sendTabTo(QAction*)));
	menu->addMenu(sendTo);
}

void TabDlg::menu_sendTabTo(QAction *act)
{
	queuedSendTabTo(static_cast<TabbableWidget*>(tabWidget_->currentPage()), act->data().value<TabDlg*>());
}

void TabDlg::sendTabTo(TabbableWidget* tab, TabDlg* otherTabs)
{
	Q_ASSERT(otherTabs);
	if (otherTabs == this)
		return;
	closeTab(tab, false);
	otherTabs->addTab(tab);
}

void TabDlg::queuedSendTabTo(TabbableWidget* tab, TabDlg *dest)
{
	Q_ASSERT(tab);
	Q_ASSERT(dest);
	QMetaObject::invokeMethod(this, "sendTabTo", Qt::QueuedConnection, Q_ARG(TabbableWidget*, tab), Q_ARG(TabDlg*, dest));
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
	tabWidget_->setTabPosition(QTabWidget::Top);
	if (option.putTabsAtBottom)
		tabWidget_->setTabPosition(QTabWidget::Bottom);

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);
}

void TabDlg::tabSelected(QWidget* _selected)
{
	TabbableWidget* selected = qobject_cast<TabbableWidget*>(_selected);
	Q_ASSERT(selected);
	if (!selectedTab_.isNull()) {
		selectedTab_->deactivated();
	}

	selectedTab_ = selected;
	Q_ASSERT(!selectedTab_.isNull());
	selected->activated();

	updateCaption();
}

bool TabDlg::managesTab(const TabbableWidget* tab) const
{
	return tabs_.contains(const_cast<TabbableWidget*>(tab));
}

bool TabDlg::tabOnTop(const TabbableWidget* tab) const
{
	return tabWidget_->currentPage() == tab;
}

void TabDlg::addTab(TabbableWidget* tab)
{
	setUpdatesEnabled(false);
	tabs_.append(tab);
	tabWidget_->addTab(tab, captionForTab(tab));

	connect(tab, SIGNAL(invalidateTabInfo()), SLOT(updateTab()));
	connect(tab, SIGNAL(updateFlashState()), SLOT(updateFlashState()));

	this->show();
	updateTab(tab);
	setUpdatesEnabled(true);
}

void TabDlg::detachCurrentTab()
{
	detachTab(static_cast<TabbableWidget*>(tabWidget_->currentPage()));
}

void TabDlg::mouseDoubleClickTab(QWidget* widget)
{
	detachTab(static_cast<TabbableWidget*>(widget));
}

void TabDlg::detachTab(TabbableWidget* tab)
{
	if (tabWidget_->count() == 1 || !tab)
		return;

	TabDlg *newTab = tabManager_->newTabs();
	sendTabTo(tab, newTab);
}

/**
 * Call this when you want a tab to be removed immediately with no readiness checks
 * or reparenting, hiding etc (Such as on tab destruction).
 */ 
void TabDlg::removeTabWithNoChecks(TabbableWidget *tab)
{
	disconnect(tab, SIGNAL(invalidateTabInfo()), this, SLOT(updateTab()));
	disconnect(tab, SIGNAL(updateFlashState()), this, SLOT(updateFlashState()));

	tabs_.removeAll(tab);
	tabWidget_->removePage(tab);
	checkHasChats();
}

/**
 * Removes the chat from the tabset, 'closing' it if specified.
 * The method is used without closing tabs when transferring from one
 * tabset to another.
 * \param chat Chat to remove.
 * \param doclose Whether the chat is 'closed' while removing it.
 */ 
void TabDlg::closeTab(TabbableWidget* chat, bool doclose)
{
	if (!chat || (doclose && !chat->readyToHide())) {
		return;
	}
	setUpdatesEnabled(false);
	chat->hide();
	removeTabWithNoChecks(chat);
	chat->reparent(0,QPoint());
	if (tabWidget_->count() > 0) {
		updateCaption();
	}
	//moved to NoChecks
	//checkHasChats();
	if (doclose && chat->testAttribute(Qt::WA_DeleteOnClose)) {
		chat->close();
	}
	setUpdatesEnabled(true);
}

void TabDlg::selectTab(TabbableWidget* chat)
{
	setUpdatesEnabled(false);
	tabWidget_->showPage(chat);
	setUpdatesEnabled(true);
}

void TabDlg::checkHasChats()
{
	if (tabWidget_->count() > 0)
		return;
	deleteLater();
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
	extinguishFlashingTabs();
}

QString TabDlg::desiredCaption() const
{
	QString cap = "";
	uint pending = 0;
	foreach(TabbableWidget* tab, tabs_) {
		pending += tab->unreadMessageCount();
	}
	if (pending > 0) {
		cap += "* ";
		if (pending > 1) {
			cap += QString("[%1] ").arg(pending);
		}
	}
	if (tabWidget_->currentPage()) {
		cap += qobject_cast<TabbableWidget*>(tabWidget_->currentPage())->getDisplayName();
		if (qobject_cast<TabbableWidget*>(tabWidget_->currentPage())->state() == TabbableWidget::StateComposing) {
			cap += tr(" is composing");
		}
	}
	return cap;
}

void TabDlg::updateCaption()
{
	setWindowTitle(desiredCaption());
}

void TabDlg::closeEvent(QCloseEvent* closeEvent)
{
	Q_UNUSED(closeEvent);
	foreach(TabbableWidget* tab, tabs_) {
		closeTab(tab);
	}
}

TabbableWidget *TabDlg::getTab(int i) const
{
	return static_cast<TabbableWidget*>(tabWidget_->page(i));
}

TabbableWidget* TabDlg::getTabPointer(QString fullJid)
{
	foreach(TabbableWidget* tab, tabs_) {
		if (tab->jid().full() == fullJid) {
			return tab;
		}
	}

	return 0;
}

void TabDlg::updateTab()
{
	TabbableWidget *tab = qobject_cast<TabbableWidget*>(sender());
	updateTab(tab);
}

QString TabDlg::captionForTab(TabbableWidget* tab) const
{
	QString label, prefix;
	if (!tab->unreadMessageCount()) {
		prefix = "";
	}
	else if (tab->unreadMessageCount() == 1) {
		prefix = "* ";
	}
	else {
		prefix = QString("[%1] ").arg(tab->unreadMessageCount());
	}

	label = prefix + tab->getDisplayName();
	label.replace("&", "&&");
	return label;
}

void TabDlg::updateTab(TabbableWidget* chat)
{
	tabWidget_->setTabLabel(chat, captionForTab(chat));
	//now set text colour based upon whether there are new messages/composing etc

	if (chat->state() == TabbableWidget::StateComposing) {
		tabWidget_->setTabTextColor(chat, Qt::darkGreen);
	}
	else if (chat->unreadMessageCount()) {
		tabWidget_->setTabTextColor(chat, Qt::red);
	}
	else {
		tabWidget_->setTabTextColor(chat, colorGroup().foreground());
	}
	updateCaption();
}

void TabDlg::nextTab()
{
	int page = tabWidget_->currentPageIndex()+1;
	if ( page >= tabWidget_->count() )
		page = 0;
	tabWidget_->setCurrentPage( page );
}

void TabDlg::previousTab()
{
	int page = tabWidget_->currentPageIndex()-1;
	if ( page < 0 )
		page = tabWidget_->count() - 1;
	tabWidget_->setCurrentPage( page );
}

void TabDlg::closeCurrentTab()
{
	closeTab(static_cast<TabbableWidget*>(tabWidget_->currentPage()));
}

void TabDlg::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Escape) {
		closeCurrentTab();
	}
	else if (e->key() == Qt::Key_W && (e->modifiers() & Qt::ControlModifier)) {
		closeCurrentTab();
	}
	else {
		e->ignore();
	}
}

void TabDlg::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat(psiTabDragMimeType)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
	}
}

void TabDlg::dropEvent(QDropEvent *event)
{
	if (!event->mimeData()->hasFormat(psiTabDragMimeType)) {
		return;
	}
	QByteArray data = event->mimeData()->data(psiTabDragMimeType);

	int remoteTab = data.toInt();
	event->acceptProposedAction();
	//the event's been and gone, now do something about it
	PsiTabBar* source = dynamic_cast<PsiTabBar*>(event->source());
	if (source) {
		PsiTabWidget* barParent = source->psiTabWidget();
		QWidget* widget = barParent->widget(remoteTab);
		TabbableWidget* chat = dynamic_cast<TabbableWidget*>(widget);
		TabDlg *dlg = tabManager_->getManagingTabs(chat);
		if (!chat || !dlg)
			return;
		dlg->queuedSendTabTo(chat, this);
	}
}

void TabDlg::extinguishFlashingTabs()
{
	foreach(TabbableWidget* tab, tabs_) {
		if (tab->flashing()) {
			tab->blockSignals(true);
			tab->doFlash(false);
			tab->blockSignals(false);
		}
	}

	updateFlashState();
}

void TabDlg::updateFlashState()
{
	bool flash = false;
	foreach(TabbableWidget* tab, tabs_) {
		if (tab->flashing()) {
			flash = true;
			break;
		}
	}

	flash = flash && !isActiveWindow();
	doFlash(flash);
}
