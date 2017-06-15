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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "tabdlg.h"

#include "iconwidget.h"
#include "iconset.h"
#include "psicon.h"

#include <QMenuBar>
#include <QCursor>
#include <QVBoxLayout>
#include <QDragMoveEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QSignalMapper>
#include <QTimer>
#include <QMimeData>

#include "psitabwidget.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "chatdlg.h"
#include "tabmanager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

//----------------------------------------------------------------------------
// TabDlgDelegate
//----------------------------------------------------------------------------

TabDlgDelegate::TabDlgDelegate(QObject *parent)
	: QObject(parent)
{
}

TabDlgDelegate::~TabDlgDelegate()
{
}

Qt::WindowFlags TabDlgDelegate::initWindowFlags() const
{
	return (Qt::WindowFlags)0;
}

void TabDlgDelegate::create(QWidget *)
{
}

void TabDlgDelegate::destroy(QWidget *)
{
}

void TabDlgDelegate::tabWidgetCreated(QWidget *, PsiTabWidget *)
{
}

bool TabDlgDelegate::paintEvent(QWidget *, QPaintEvent *)
{
	return false;
}

bool TabDlgDelegate::resizeEvent(QWidget *, QResizeEvent *)
{
	return false;
}

bool TabDlgDelegate::mousePressEvent(QWidget *, QMouseEvent *)
{
	return false;
}

bool TabDlgDelegate::mouseMoveEvent(QWidget *, QMouseEvent *)
{
	return false;
}

bool TabDlgDelegate::mouseReleaseEvent(QWidget *, QMouseEvent *)
{
	return false;
}

bool TabDlgDelegate::changeEvent(QWidget *, QEvent *)
{
	return false;
}

bool TabDlgDelegate::tabEvent(QWidget *, QEvent *)
{
	return false;
}

bool TabDlgDelegate::tabEventFilter(QWidget *, QObject *, QEvent *)
{
	return false;
}

//----------------------------------------------------------------------------
// TabDlg
//----------------------------------------------------------------------------

/**
 * Constructs a TabDlg
 *
 * \param tabManager The tabManager that will manage this TabDlg
 * \param delegate If non-zero, this is a pointer to a TabDlgDelegate that
 *        will manage some aspects of the TabDlg behavior.  Ownership is not
 *        passed.
 */
TabDlg::TabDlg(TabManager* tabManager, const QString& geometryOption, TabDlgDelegate *delegate)
		: AdvancedWidget<QWidget>(0, delegate ? delegate->initWindowFlags() : (Qt::WindowFlags)0)
		, delegate_(delegate)
		, tabWidget_(0)
		, detachButton_(0)
		, closeButton_(0)
		, closeCross_(0)
		, tabMenu_(new QMenu(this))
		, act_close_(0)
		, act_next_(0)
		, act_prev_(0)
		, tabManager_(tabManager)
		, userManagement_(true)
		, tabBarSingles_(true)
		, simplifiedCaption_(false)
		, activateTabMapper_(0)
{
	if (delegate_) {
		delegate_->create(this);
	}

	// FIXME
	qRegisterMetaType<TabDlg*>("TabDlg*");
	qRegisterMetaType<TabbableWidget*>("TabbableWidget*");

	tabWidget_ = new PsiTabWidget(this);
	tabWidget_->setCloseIcon(IconsetFactory::icon("psi/closetab").icon());
	connect(tabWidget_, SIGNAL(mouseDoubleClickTab(QWidget*)), SLOT(mouseDoubleClickTab(QWidget*)));
	connect(tabWidget_, SIGNAL(mouseMiddleClickTab(QWidget*)), SLOT(mouseMiddleClickTab(QWidget*)));
	connect(tabWidget_, SIGNAL(aboutToShowMenu(QMenu*)), SLOT(tab_aboutToShowMenu(QMenu*)));
	connect(tabWidget_, SIGNAL(tabContextMenu(int, QPoint, QContextMenuEvent*)), SLOT(showTabMenu(int, QPoint, QContextMenuEvent*)));
	connect(tabWidget_, SIGNAL(closeButtonClicked()), SLOT(closeCurrentTab()));
	connect(tabWidget_, SIGNAL(currentChanged(QWidget*)), SLOT(tabSelected(QWidget*)));
	connect(tabWidget_, SIGNAL(tabCloseRequested(int)), SLOT(tabCloseRequested(int)));

	if(delegate_)
		delegate_->tabWidgetCreated(this, tabWidget_);

	QVBoxLayout *vert1 = new QVBoxLayout(this);
	vert1->setMargin(1);
	vert1->addWidget(tabWidget_);

	setAcceptDrops(true);

	X11WM_CLASS("tabs");
	setLooks();

	act_close_ = new QAction(this);
	addAction(act_close_);
	connect(act_close_,SIGNAL(triggered()), SLOT(closeCurrentTab()));
	act_prev_ = new QAction(this);
	addAction(act_prev_);
	connect(act_prev_,SIGNAL(triggered()), SLOT(previousTab()));
	act_next_ = new QAction(this);
	addAction(act_next_);
	connect(act_next_,SIGNAL(triggered()), SLOT(nextTab()));

	setShortcuts();

	if(!PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString().contains('A'))
		setGeometryOptionPath(geometryOption);
}

TabDlg::~TabDlg()
{
	// TODO: make sure that TabDlg is properly closed and its closeEvent() is invoked,
	// so it could cancel an application quit
	// Q_ASSERT(tabs_.isEmpty());

	// ensure all tabs are closed at this moment
	foreach(TabbableWidget* tab, tabs_) {
		delete tab;
	}

	if (delegate_) {
		delegate_->destroy(this);
	}
}

#ifndef HAVE_QT5
// FIXME: This is a bad idea to store pointers in QMimeData
Q_DECLARE_METATYPE(TabDlg*);
Q_DECLARE_METATYPE(TabbableWidget*);
#endif

void TabDlg::setShortcuts()
{
	act_close_->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	act_prev_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.previous-tab"));
	act_next_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.next-tab"));

	bool useTabShortcuts = PsiOptions::instance()->getOption("options.ui.tabs.use-tab-shortcuts").toBool();
	if (useTabShortcuts && !activateTabMapper_) {
		activateTabMapper_ = new QSignalMapper(this);
		connect(activateTabMapper_, SIGNAL(mapped(int)), tabWidget_, SLOT(setCurrentPage(int)));
		for (int i = 0; i < 10; ++i) {
			QAction* action = new QAction(this);
			connect(action, SIGNAL(triggered()), activateTabMapper_, SLOT(map()));
			action->setShortcuts(QList<QKeySequence>() << QKeySequence(QString("Ctrl+%1").arg(i))
													   << QKeySequence(QString("Alt+%1").arg(i)));
			activateTabMapper_->setMapping(action, (i > 0 ? i : 10) - 1);
			tabMapperActions_ += action;
			addAction(action);
		}
	}
	else if (!useTabShortcuts && activateTabMapper_) {
		qDeleteAll(tabMapperActions_);
		tabMapperActions_.clear();
		delete activateTabMapper_;
		activateTabMapper_ = 0;
	}
}

void TabDlg::resizeEvent(QResizeEvent *e)
{
	AdvancedWidget<QWidget>::resizeEvent(e);

	emit resized(e->size());

	// delegate may want to act on resize event
	if (delegate_) {
		delegate_->resizeEvent(this, e);
	}
}

void TabDlg::showTabMenu(int tab, QPoint pos, QContextMenuEvent * event)
{
	Q_UNUSED(event);
	clearMenu(tabMenu_);

	if (tab != -1) {
		QAction *d = 0;
		QAction *h = tabMenu_->addAction(tr("Hide Tab"));
		if(userManagement_) {
			d = tabMenu_->addAction(tr("Detach Tab"));
		}

		QAction *c = tabMenu_->addAction(tr("Close Tab"));

		QMap<QAction*, TabDlg*> sentTos;
		if(userManagement_) {
			QMenu* sendTo = new QMenu(tabMenu_);
			sendTo->setTitle(tr("Send Tab To"));
			foreach(TabDlg* tabSet, tabManager_->tabSets()) {
				QAction *act = sendTo->addAction(tabSet->desiredCaption());
				if (tabSet == this)
					act->setEnabled(false);
				sentTos[act] = tabSet;
			}
			tabMenu_->addMenu(sendTo);
		}

		QAction *act = tabMenu_->exec(pos);
		if (!act)
			return;
		if (act == c) {
			closeTab(getTab(tab));
		}
		else if (act == d) {
			detachTab(getTab(tab));
		}
		else if(act == h) {
			hideTab(getTab(tab));
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
	menu->addAction(tr("Hide Current Tab"), this, SLOT(hideCurrentTab()));
	menu->addAction(tr("Hide All Tabs"), this, SLOT(hideAllTab()));
	menu->addAction(tr("Detach Current Tab"), this, SLOT(detachCurrentTab()));
	menu->addAction(tr("Close Current Tab"), this, SLOT(closeCurrentTab()));

	QMenu* sendTo = new QMenu(menu);
	sendTo->setTitle(tr("Send Current Tab To"));
	int tabDlgMetaType = qRegisterMetaType<TabDlg*>("TabDlg*");
	foreach(TabDlg* tabSet, tabManager_->tabSets()) {
		QAction *act = sendTo->addAction(tabSet->desiredCaption());
		act->setData(QVariant(tabDlgMetaType, &tabSet));
		act->setEnabled(tabSet != this);
	}
	connect(sendTo, SIGNAL(triggered(QAction*)), SLOT(menu_sendTabTo(QAction*)));
	menu->addMenu(sendTo);
	menu->addSeparator();

	QAction *act;
	act = menu->addAction(tr("Use for New Chats"), this, SLOT(setAsDefaultForChat()));
	act->setCheckable(true);
	act->setChecked(tabManager_->preferredTabsForKind('C') == this);
	act = menu->addAction(tr("Use for New Mucs"), this, SLOT(setAsDefaultForMuc()));
	act->setCheckable(true);
	act->setChecked(tabManager_->preferredTabsForKind('M') == this);
}

void TabDlg::setAsDefaultForChat() {
	tabManager_->setPreferredTabsForKind('C', this);
}
void TabDlg::setAsDefaultForMuc() {
	tabManager_->setPreferredTabsForKind('M', this);
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
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/start-chat").icon());
#endif
	tabWidget_->setTabPosition(QTabWidget::North);
	if (PsiOptions::instance()->getOption("options.ui.tabs.put-tabs-at-bottom").toBool())
		tabWidget_->setTabPosition(QTabWidget::South);

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);

	const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
	if (!css.isEmpty()) {
		setStyleSheet(css);
	}
}

void TabDlg::tabSelected(QWidget* _selected)
{
	// _selected could be null when TabDlg is closing and deleting all its tabs
	TabbableWidget* selected = _selected ? qobject_cast<TabbableWidget*>(_selected) : 0;
	if (!selectedTab_.isNull()) {
		selectedTab_->deactivated();
	}

	selectedTab_ = selected;
	if (selected) {
		selected->activated();
	}

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
	tabWidget_->addTab(tab, captionForTab(tab), tab->icon());

	connect(tab, SIGNAL(invalidateTabInfo()), SLOT(updateTab()));
	connect(tab, SIGNAL(updateFlashState()), SLOT(updateFlashState()));

	updateTab(tab);
	setUpdatesEnabled(true);
	QTimer::singleShot(0, this, SLOT(showTabWithoutActivation()));
}

void TabDlg::showTabWithoutActivation()
{
	showWithoutActivation();
}

void TabDlg::hideCurrentTab()
{
	hideTab(static_cast<TabbableWidget*>(tabWidget_->currentPage()));
}

void TabDlg::hideTab(TabbableWidget* tab)
{
	closeTab(tab, false);
}

void TabDlg::hideAllTab()
{
	foreach(TabbableWidget* tab, tabs_)
		hideTab(tab);
}

void TabDlg::detachCurrentTab()
{
	detachTab(static_cast<TabbableWidget*>(tabWidget_->currentPage()));
}

void TabDlg::mouseDoubleClickTab(QWidget* widget)
{
	const QString act = PsiOptions::instance()->getOption("options.ui.tabs.mouse-doubleclick-action").toString();
	if(act == "hide")
		hideTab(static_cast<TabbableWidget*>(widget));
	else if(act == "close")
		closeTab(static_cast<TabbableWidget*>(widget));
	else if(act == "detach" && userManagement_)
		detachTab(static_cast<TabbableWidget*>(widget));
}

void TabDlg::mouseMiddleClickTab(QWidget* widget) {
	const QString act = PsiOptions::instance()->getOption("options.ui.tabs.mouse-middle-button").toString();
	if(act == "hide")
		hideTab(static_cast<TabbableWidget*>(widget));
	else if(act == "close")
		closeTab(static_cast<TabbableWidget*>(widget));
	else if(act == "detach" && userManagement_)
		detachTab(static_cast<TabbableWidget*>(widget));
}

void TabDlg::detachTab(TabbableWidget* tab)
{
	if (tabWidget_->count() == 1 || !tab)
		return;

	TabDlg *newTab = tabManager_->newTabs(tab);
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
	chat->setParent(0);
	chat->deactivated();
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
	if (tabWidget_->count() > 0 || this != window())
		return;
	if (tabs_.count() > 0) {
		hide();
		return;
	}
	deleteLater();
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
		if (!simplifiedCaption_ && pending > 1) {
			cap += QString("[%1] ").arg(pending);
		}
	}

	if (tabWidget_->currentPage()) {
		if (simplifiedCaption_ && tabs_.count() > 1) {
			cap += tr("%1 Conversations").arg(tabs_.count());
		} else {
			cap += qobject_cast<TabbableWidget*>(tabWidget_->currentPage())->getDisplayName();
			if (qobject_cast<TabbableWidget*>(tabWidget_->currentPage())->state() == TabbableWidget::StateComposing) {
				cap += tr(" is composing");
			}
		}
	}
	return cap;
}

void TabDlg::updateCaption()
{
	setWindowTitle(desiredCaption());

	// FIXME: this probably shouldn't be in here, but it works easily
	updateTabBar();
}

void TabDlg::closeEvent(QCloseEvent* closeEvent)
{
	foreach(TabbableWidget* tab, tabs_) {
		if (!tab->readyToHide()) {
			closeEvent->ignore();
			return;
		}
	}
	if(PsiOptions::instance()->getOption("options.ui.chat.hide-when-closing").toBool()) {
		hide();
	}
	else {
		foreach(TabbableWidget* tab, tabs_) {
			closeTab(tab);
		}
	}
}

TabbableWidget *TabDlg::getTab(int i) const
{
	return static_cast<TabbableWidget*>(tabWidget_->page(i));
}

TabbableWidget* TabDlg::getTabPointer(PsiAccount* account, QString fullJid)
{
	foreach(TabbableWidget* tab, tabs_) {
		if (tab->jid().full() == fullJid && tab->account() == account) {
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
	tabWidget_->setTabText(chat, captionForTab(chat));
	//now set text colour based upon whether there are new messages/composing etc

	if (chat->state() == TabbableWidget::StateComposing) {
		tabWidget_->setTabTextColor(chat, Qt::darkGreen);
		tabWidget_->setTabIcon(chat, IconsetFactory::iconPtr("psi/typing")->icon());
	}
	else if (chat->unreadMessageCount()) {
		tabWidget_->setTabTextColor(chat, Qt::red);
		tabWidget_->setTabIcon(chat, IconsetFactory::iconPtr("psi/chat")->icon());
	}
	else {
		tabWidget_->setTabTextColor(chat, palette().windowText().color());
		tabWidget_->setTabIcon(chat, chat->icon());
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

void TabDlg::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat(PSITABDRAGMIMETYPE)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
	}
}

void TabDlg::dropEvent(QDropEvent *event)
{
	if (!event->mimeData()->hasFormat(PSITABDRAGMIMETYPE)) {
		return;
	}
	QByteArray data = event->mimeData()->data(PSITABDRAGMIMETYPE);

	int remoteTab = data.toInt();
	event->acceptProposedAction();
	//the event's been and gone, now do something about it
	PsiTabBar* source = dynamic_cast<PsiTabBar*>(event->source());
	if (source) {
		PsiTabWidget* barParent = source->psiTabWidget();
		if (remoteTab >= barParent->count()) return;
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

void TabDlg::paintEvent(QPaintEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->paintEvent(this, event)) {
		return;
	} else {
		AdvancedWidget<QWidget>::paintEvent(event);
	}
}

void TabDlg::mousePressEvent(QMouseEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->mousePressEvent(this, event)) {
		return;
	} else {
		AdvancedWidget<QWidget>::mousePressEvent(event);
	}
}

void TabDlg::mouseMoveEvent(QMouseEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->mouseMoveEvent(this, event)) {
		return;
	} else {
		AdvancedWidget<QWidget>::mouseMoveEvent(event);
	}
}

void TabDlg::mouseReleaseEvent(QMouseEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->mouseReleaseEvent(this, event)) {
		return;
	} else {
		AdvancedWidget<QWidget>::mouseReleaseEvent(event);
	}
}

void TabDlg::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::ActivationChange ||
		event->type() == QEvent::WindowStateChange)
	{
		if (tabWidget_->currentPage()) {
			QCoreApplication::sendEvent(tabWidget_->currentPage(), event);
		}

		if (isActiveWindow()) {
			activated();
		}
	}

	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->changeEvent(this, event)) {
		return;
	}
	else {
		AdvancedWidget<QWidget>::changeEvent(event);
	}
}

bool TabDlg::event(QEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->tabEvent(this, event)) {
		return true;
	} else {
		return AdvancedWidget<QWidget>::event(event);
	}
}

bool TabDlg::eventFilter(QObject *obj, QEvent *event)
{
	// delegate if possible, otherwise use default
	if (delegate_ && delegate_->tabEventFilter(this, obj, event)) {
		return true;
	} else {
		return AdvancedWidget<QWidget>::eventFilter(obj, event);
	}
}

int TabDlg::tabCount() const
{
	return tabs_.count();
}

void TabDlg::setUserManagementEnabled(bool enabled)
{
	if (userManagement_ == enabled) {
		return;
	}

	userManagement_ = enabled;
	tabWidget_->setTabButtonsShown(enabled);
	tabWidget_->setDragsEnabled(enabled);
}

void TabDlg::setTabBarShownForSingles(bool enabled)
{
	if (tabBarSingles_ == enabled) {
		return;
	}

	tabBarSingles_ = enabled;
	updateTabBar();
}

void TabDlg::updateTabBar()
{
	if (tabBarSingles_) {
		tabWidget_->setTabBarShown(true);
	} else {
		if (tabWidget_->count() > 1)
			tabWidget_->setTabBarShown(true);
		else
			tabWidget_->setTabBarShown(false);
	}
}

void TabDlg::setSimplifiedCaptionEnabled(bool enabled)
{
	if (simplifiedCaption_ == enabled) {
		return;
	}

	simplifiedCaption_ = enabled;
	updateCaption();
}

/**
  * the slot is invoked, when small close button is clicked on a tab
  * dont close tabs, that are not active.
  * \param tab number requested to close
  */
void TabDlg::tabCloseRequested(int i)
{
	if (tabWidget_->currentPageIndex() != i)
		if (!PsiOptions::instance()->getOption("options.ui.tabs.can-close-inactive-tab").toBool()) {
			selectTab(static_cast<TabbableWidget*>(tabWidget_->page(i)));
			return;
		}

	if (PsiOptions::instance()->getOption("options.ui.chat.hide-when-closing").toBool())
		hideTab(static_cast<TabbableWidget*>(tabWidget_->page(i)));
	else
		closeTab(static_cast<TabbableWidget*>(tabWidget_->page(i)));
}

/**
 * Set the icon of the tab.
 */
void TabDlg::setTabIcon(QWidget *widget,const QIcon &icon)
{
	tabWidget_->setTabIcon(widget, icon);
}

TabbableWidget* TabDlg::getCurrentTab() const
{
	return dynamic_cast<TabbableWidget*>(tabWidget_->currentPage());
}
