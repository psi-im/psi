#include "tabmanager.h"
#include "tabdlg.h"
#include "tabbable.h"

TabManager::TabManager(PsiCon* psiCon, QObject *parent) : QObject(parent), psiCon_(psiCon) {
	//the list 'owns' the tabs
	tabs_.setAutoDelete( true );
	tabControlledChats_.setAutoDelete( false );
}

TabManager::~TabManager() {
	
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

bool TabManager::isChatTabbed(const Tabbable* chat) const
{
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->managesTab(chat)) {
			return true;
		}
	}
	return false;
}

Tabbable* TabManager::getChatInTabs(QString jid)
{
	foreach(TabDlg* tabDlg, tabs_) {
		if (tabDlg->getTabPointer(jid)) {
			return tabDlg->getTabPointer(jid);
		}
	}
	return 0;

}

TabDlg* TabManager::getManagingTabs(const Tabbable* chat) const
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
