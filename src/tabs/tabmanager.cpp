#include "tabmanager.h"
#include "tabdlg.h"
#include "tabbablewidget.h"
#include "groupchatdlg.h"
#include "chatdlg.h"

TabManager::TabManager(PsiCon* psiCon, QObject *parent) : QObject(parent), psiCon_(psiCon) {
	//the list 'owns' the tabs
	tabs_.setAutoDelete( true );
	tabControlledChats_.setAutoDelete( false );
}

TabManager::~TabManager() {
	
}

PsiCon* TabManager::psiCon() const
{
	return psiCon_;
}

TabDlg* TabManager::getTabs()
{
	if (!tabs_.isEmpty())
	{
		return tabs_.getFirst();
	}
	else
	{
		return newTabs();
	}
}

bool TabManager::shouldBeTabbed(QWidget *widget) {
	qDebug("Checking if widget should be tabbed");
	if (!option.useTabs)
	{
		qDebug("Tabs disabled");
		return false;
	}
	if (qobject_cast<ChatDlg*> (widget)) {
		qDebug("Casts to ChatDlg");
		return true;
	}
	if (qobject_cast<GCMainDlg*> (widget)) {
		qDebug("Casts to GCMainDlg");
		return true;
	}
	qDebug("Unknown type");
	return false;
}

TabDlg* TabManager::newTabs()
{
	TabDlg *tab = new TabDlg(this);
	tabs_.append(tab);
	connect (tab, SIGNAL ( isDying(TabDlg*) ), SLOT ( tabDying(TabDlg*) ) );
	connect (psiCon_, SIGNAL(emitOptionsUpdate()), tab, SLOT(optionsUpdate()));
	return tab;
}

void TabManager::tabDying(TabDlg* tab)
{
	tabs_.remove(tab);
}

bool TabManager::isChatTabbed(const TabbableWidget* chat) const
{
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->managesTab(chat)) {
			return true;
		}
	}
	return false;
}

TabbableWidget* TabManager::getChatInTabs(QString jid)
{
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->getTabPointer(jid)) {
			return tabDlg->getTabPointer(jid);
		}
	}
	return 0;

}

TabDlg* TabManager::getManagingTabs(const TabbableWidget* chat) const
{
	//FIXME: this looks like it could be broken to me (KIS)
	//Does this mean that opening two chats to the same jid will go wrong?
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->managesTab(chat)) {
			return tabDlg;
		}
	}
	return 0;

}

Q3PtrList<TabDlg>* TabManager::getTabSets()
{
	return &tabs_;
}

void TabManager::deleteAll()
{
	tabs_.clear();
}
