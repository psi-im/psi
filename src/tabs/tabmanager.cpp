#include "tabmanager.h"

#include <QtAlgorithms>

#include "tabdlg.h"
#include "tabbablewidget.h"
#include "groupchatdlg.h"
#include "chatdlg.h"

TabManager::TabManager(PsiCon* psiCon, QObject *parent)
	: QObject(parent)
	, psiCon_(psiCon)
{
}

TabManager::~TabManager()
{
	qDeleteAll(tabs_);
}

PsiCon* TabManager::psiCon() const
{
	return psiCon_;
}

TabDlg* TabManager::getTabs()
{
	if (!tabs_.isEmpty()) {
		return tabs_.first();
	}
	else {
		return newTabs();
	}
}

bool TabManager::shouldBeTabbed(QWidget *widget)
{
	if (!option.useTabs) {
		return false;
	}
	if (qobject_cast<ChatDlg*> (widget)) {
		return true;
	}
	if (qobject_cast<GCMainDlg*> (widget)) {
		return true;
	}
	qDebug("Checking if widget should be tabbed: Unknown type");
	return false;
}

TabDlg* TabManager::newTabs()
{
	TabDlg *tab = new TabDlg(this);
	tabs_.append(tab);
	connect(tab, SIGNAL(destroyed(QObject*)), SLOT(tabDestroyed(QObject*)));
	connect(psiCon_, SIGNAL(emitOptionsUpdate()), tab, SLOT(optionsUpdate()));
	return tab;
}

void TabManager::tabDestroyed(QObject* obj)
{
	Q_ASSERT(tabs_.contains(static_cast<TabDlg*>(obj)));
	tabs_.removeAll(static_cast<TabDlg*>(obj));
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

const QList<TabDlg*>& TabManager::tabSets()
{
	return tabs_;
}

void TabManager::deleteAll()
{
	tabs_.clear();
}
