#include "tabmanager.h"

#include "chatdlg.h"
#include "groupchatdlg.h"
#include "psioptions.h"
#include "tabbablewidget.h"
#include "tabdlg.h"

#include <QtAlgorithms>

TabManager::TabManager(PsiCon *psiCon, QObject *parent) :
    QObject(parent), psiCon_(psiCon), tabDlgDelegate_(nullptr), userManagement_(true), tabSingles_(true),
    simplifiedCaption_(false)
{
}

TabManager::~TabManager() { deleteAll(); }

PsiCon *TabManager::psiCon() const { return psiCon_; }

TabDlg *TabManager::getTabs(QWidget *widget)
{
    QChar kind = tabKind(widget);
    if (preferedTabsetForKind_.contains(kind)) {
        return preferedTabsetForKind_[kind];
    } else {
        return newTabs(widget);
    }
}

QChar TabManager::tabKind(QWidget *widget)
{
    QChar retval = 0;
    if (widget) {
        const QString name = widget->objectName();
        if (name == "ChatDlg") {
            retval = 'C';
        } else if (name == "GroupChatDlg") {
            retval = 'M';
        } else {
            qDebug("Checking if widget should be tabbed: Unknown type");
        }
    }
    return retval;
}

bool TabManager::shouldBeTabbed(QWidget *widget)
{
    if (!PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool()) {
        return false;
    }

    QString grouping = PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString();
    if (grouping.contains(tabKind(widget))) {
        return true;
    }
    return false;
}

TabDlg *TabManager::newTabs(QWidget *widget)
{
    QChar   kind = tabKind(widget);
    QString group, grouping = PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString();
    foreach (QString g, grouping.split(':')) {
        if (g.contains(kind)) {
            group = g;
            break;
        }
    }

    QString geometryOption = QString("options.ui.tabs.group-state.%1.size").arg(group);

    TabDlg *tab = new TabDlg(this, geometryOption, tabDlgDelegate_);
    tab->setUserManagementEnabled(userManagement_);
    tab->setTabBarShownForSingles(tabSingles_);
    tab->setSimplifiedCaptionEnabled(simplifiedCaption_);
    tabsetToKinds_.insert(tab, group);
    for (int i = 0; i < group.length(); i++) {
        QChar k = group.at(i);
        if (!preferedTabsetForKind_.contains(k)) {
            preferedTabsetForKind_.insert(k, tab);
        }
    }
    tabs_.append(tab);
    connect(tab, SIGNAL(destroyed(QObject *)), SLOT(tabDestroyed(QObject *)));
    connect(psiCon_, SIGNAL(emitOptionsUpdate()), tab, SLOT(optionsUpdate()));
    return tab;
}

void TabManager::tabDestroyed(QObject *obj)
{
    Q_ASSERT(tabs_.contains(static_cast<TabDlg *>(obj)));
    tabs_.removeAll(static_cast<TabDlg *>(obj));
    tabsetToKinds_.remove(static_cast<TabDlg *>(obj));
    QMutableMapIterator<QChar, TabDlg *> it(preferedTabsetForKind_);
    while (it.hasNext()) {
        it.next();
        if (preferedTabsetForKind_[it.key()] != obj)
            continue;
        bool ok = false;
        foreach (TabDlg *tabDlg, tabs_) {
            // currently destroyed tab is removed from the list a few lines above
            if (tabsetToKinds_[tabDlg].contains(it.key())) {
                preferedTabsetForKind_[it.key()] = tabDlg;
                ok                               = true;
                break;
            }
        }
        if (!ok)
            it.remove();
    }
}

TabDlg *TabManager::preferredTabsForKind(QChar kind) { return preferedTabsetForKind_.value(kind); }

void TabManager::setPreferredTabsForKind(QChar kind, TabDlg *tab)
{
    Q_ASSERT(tabs_.contains(tab));
    preferedTabsetForKind_[kind] = tab;
}

bool TabManager::isChatTabbed(const TabbableWidget *chat) const
{
    foreach (TabDlg *tabDlg, tabs_) {
        if (tabDlg->managesTab(chat)) {
            return true;
        }
    }
    return false;
}

TabDlg *TabManager::getManagingTabs(const TabbableWidget *chat) const
{
    // FIXME: this looks like it could be broken to me (KIS)
    // Does this mean that opening two chats to the same jid will go wrong?
    foreach (TabDlg *tabDlg, tabs_) {
        if (tabDlg->managesTab(chat)) {
            return tabDlg;
        }
    }
    return nullptr;
}

const QList<TabDlg *> &TabManager::tabSets() { return tabs_; }

void TabManager::deleteAll()
{
    qDeleteAll(tabs_);
    tabs_.clear();
}

void TabManager::setTabDlgDelegate(TabDlgDelegate *delegate) { tabDlgDelegate_ = delegate; }

void TabManager::setUserManagementEnabled(bool enabled)
{
    if (userManagement_ == enabled) {
        return;
    }

    userManagement_ = enabled;
    foreach (TabDlg *tab, tabs_) {
        tab->setUserManagementEnabled(enabled);
    }
}

void TabManager::setTabBarShownForSingles(bool enabled)
{
    if (tabSingles_ == enabled) {
        return;
    }

    tabSingles_ = enabled;
    foreach (TabDlg *tab, tabs_) {
        tab->setTabBarShownForSingles(enabled);
    }
}

void TabManager::setSimplifiedCaptionEnabled(bool enabled)
{
    if (simplifiedCaption_ == enabled) {
        return;
    }

    simplifiedCaption_ = enabled;
    foreach (TabDlg *tab, tabs_) {
        tab->setSimplifiedCaptionEnabled(enabled);
    }
}
