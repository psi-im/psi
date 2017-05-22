/*
 * psicontactlist.cpp - general abstraction of the psi-specific contact list
 * Copyright (C) 2006  Michail Pishchagin
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

#include "psicontactlist.h"

#include <QTimer>

#include "psiaccount.h"
#include "psievent.h"
#include "accountadddlg.h"
#include "serverinfomanager.h"
#include "psicon.h"
#include <QDebug>

/**
 * Constructs new PsiContactList. \param psi will not be PsiContactList's parent though.
 */
PsiContactList::PsiContactList(PsiCon* psi)
	: QObject()
	, psi_(psi)
	, showAgents_(false)
	, showHidden_(false)
	, showSelf_(false)
	, showOffline_(false)
	, accountsLoaded_(false)
	, contactSortStyle_("")
{
}

/**
 * Destroys the PsiContactList along with all PsiAccounts.
 */
PsiContactList::~PsiContactList()
{
	emit destroying();
	accountsLoaded_ = false;

	// PsiAccount calls some signals while being deleted prior to being unlinked,
	// which in result could cause calls to PsiContactList::accounts()
	QList<PsiAccount*> toDelete(accounts_);

	enabledAccounts_.clear();

	foreach(PsiAccount* account, toDelete)
		delete account;
}

/**
 * Returns pointer to the global Psi Controller.
 */
PsiCon* PsiContactList::psi() const
{
	return psi_;
}

/**
 * Returns list of all accounts if \param enabledOnly is false,
 * equivalent to enabledAccounts() otherwise.
 */
const QList<PsiAccount*>& PsiContactList::accounts() const
{
	return accounts_;
}

/**
 * Returns list with all enabled accounts.
 */
const QList<PsiAccount*>& PsiContactList::enabledAccounts() const
{
	return enabledAccounts_;
}

/**
 * Returns true, if there are some enabled accounts which are active.
 */
bool PsiContactList::haveActiveAccounts() const
{
	foreach(PsiAccount* account, enabledAccounts_)
		if (account->isActive())
			return true;
	return false;
}

bool PsiContactList::haveAvailableAccounts() const
{
	foreach(PsiAccount* account, enabledAccounts_)
		if (account->isAvailable())
			return true;
	return false;
}

/**
 * Returns true if enabledAccounts() list is not empty.
 */
bool PsiContactList::haveEnabledAccounts() const
{
	return !enabledAccounts_.isEmpty();
}

/**
 * Returns true if there are some accounts that are trying to establish a connection.
 */
bool PsiContactList::haveConnectingAccounts() const
{
	foreach(PsiAccount* account, enabledAccounts())
		if (account->isActive() && !account->isAvailable())
			return true;

	return false;
}

/**
 * At the moment, it returns first enabled account.
 */
PsiAccount *PsiContactList::defaultAccount() const
{
	if (!enabledAccounts_.isEmpty()) {
		return enabledAccounts_.first();
	}
	return 0;
}

/**
 * Creates new PsiAccount based on some initial settings. This is used by AccountAddDlg.
 */
PsiAccount* PsiContactList::createAccount(const QString& name, const Jid& j, const QString& pass, bool opt_host, const QString& host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl, QString proxyID, const QString &tlsOverrideDomain, const QByteArray &tlsOverrideCert)
{
	UserAccount acc;
	acc.name = name;

	acc.jid = j.full();
	if(!pass.isEmpty()) {
		acc.opt_pass = true;
		acc.pass = pass;
	}

	acc.opt_host = opt_host;
	acc.host = host;
	acc.port = port;
	acc.ssl = ssl;
	acc.proxyID = proxyID;
	acc.legacy_ssl_probe = legacy_ssl_probe;

	acc.tog_offline = showOffline();
	acc.tog_agents = showAgents();
	acc.tog_hidden = showHidden();
	acc.tog_self = showSelf();

	acc.tlsOverrideCert = tlsOverrideCert;
	acc.tlsOverrideDomain = tlsOverrideDomain;

	PsiAccount *pa = loadAccount(acc);

	emit saveAccounts();
	emit accountCreated(pa);

	return pa;
}

void PsiContactList::createAccount(const UserAccount& acc)
{
	PsiAccount *pa = loadAccount(acc);
	emit saveAccounts();
	emit accountCreated(pa);
}

/**
 * Call this to remove account completely from system.
 */
void PsiContactList::removeAccount(PsiAccount* account)
{
	emit accountRemoved(account);
	account->deleteQueueFile();
	delete account;
	emit saveAccounts();
}

/**
 * Obsolete. Call PsiAccount::setEnabled() directly.
 */
void PsiContactList::setAccountEnabled(PsiAccount* account, bool enabled)
{
	account->setEnabled(enabled);
}

/**
 * Counts total number of unread events for all accounts.
 */
int PsiContactList::queueCount() const
{
	int total = 0;
	foreach(PsiAccount* account, enabledAccounts_)
		total += account->eventQueue()->count();
	return total;
}

int PsiContactList::queueContactCount() const
{
	int total = 0;
	foreach(PsiAccount* account, enabledAccounts_)
		total += account->eventQueue()->contactCount();
	return total;
}

/**
 * Finds account with unprocessed event of highest priority, starting with
 * non-DND accounts.
 */
PsiAccount* PsiContactList::queueLowestEventId()
{
	PsiAccount *low = 0;

	// first try to get event from non-dnd account
	low = tryQueueLowestEventId(false);

	// if failed, then get the event from dnd account instead
	if (!low)
		low = tryQueueLowestEventId(true);

	return low;
}

/**
 * Creates new PsiAccount from \param acc.
 */
PsiAccount *PsiContactList::loadAccount(const UserAccount& acc)
{
	emit beginBulkContactUpdate();
	PsiAccount *pa = psi_->createAccount(acc);
	connect(pa, SIGNAL(enabledChanged()), SIGNAL(accountCountChanged()));
	emit accountAdded(pa);
	emit endBulkContactUpdate();
	return pa;
}

/**
 * Loads accounts from \param list
 */
void PsiContactList::loadAccounts(const UserAccountList &_list)
{
	UserAccountList list = _list;
	emit beginBulkContactUpdate();
	foreach(UserAccount account, list)
		loadAccount(account);
	emit endBulkContactUpdate();

	accountsLoaded_ = true;
	emit loadedAccounts();
	emit accountCountChanged();
}

/**
 * Creates new UserAccountList from all the PsiAccounts ready for saving to disk.
 */
UserAccountList PsiContactList::getUserAccountList() const
{
	UserAccountList acc;
	foreach(PsiAccount* account, accounts_) {
		acc += account->userAccount();
	}
	return acc;
}

/**
 * Moves accounts from list to top. affects default account
 */
void PsiContactList::setAccountsOrder(QList<PsiAccount*> accounts)
{
	int start = 0;
	bool needSave = false;
	foreach (PsiAccount *account, accounts) {
		int index = accounts_.indexOf(account, start);
		if (index != -1) {
			accounts_.move(index, start);
			if (index != start) {
				needSave = true;
			}
			start++;
		}
	}
	if (needSave) {
		emit saveAccounts();
	}
}

/**
 * It's called by each and every PsiAccount on its creation.
 */
void PsiContactList::link(PsiAccount* account)
{
	Q_ASSERT(!accounts_.contains(account));
	if (accounts_.contains(account))
		return;
	connect(account, SIGNAL(updatedActivity()), this, SIGNAL(accountActivityChanged()));
	connect(account->serverInfoManager(),SIGNAL(featuresChanged()), this, SIGNAL(accountFeaturesChanged()));
	connect(account, SIGNAL(queueChanged()), this, SIGNAL(queueChanged()));
	connect(account, SIGNAL(beginBulkContactUpdate()), this, SIGNAL(beginBulkContactUpdate()));
	connect(account, SIGNAL(endBulkContactUpdate()), this, SIGNAL(endBulkContactUpdate()));
	connect(account, SIGNAL(rosterRequestFinished()), this, SIGNAL(rosterRequestFinished()));
	accounts_.append(account);
	if (account->enabled())
		addEnabledAccount(account);
	connect(account, SIGNAL(enabledChanged()), SLOT(accountEnabledChanged()));
	emit accountCountChanged();
}

/**
 * It's called by each and every PsiAccount on its destruction.
 */
void PsiContactList::unlink(PsiAccount* account)
{
	Q_ASSERT(accounts_.contains(account));
	if (!accounts_.contains(account))
		return;
	disconnect(account, SIGNAL(updatedActivity()), this, SIGNAL(accountActivityChanged()));
	accounts_.removeAll(account);
	removeEnabledAccount(account);
	emit accountCountChanged();
}

/**
 * TODO
 */
bool PsiContactList::showAgents() const
{
	return showAgents_;
}

/**
 * TODO
 */
bool PsiContactList::showHidden() const
{
	return showHidden_;
}

/**
 * TODO
 */
bool PsiContactList::showSelf() const
{
	return showSelf_;
}

bool PsiContactList::showOffline() const
{
	return showOffline_;
}

QString PsiContactList::contactSortStyle() const
{
	return contactSortStyle_;
}

/**
 * TODO
 */
void PsiContactList::setShowAgents(bool showAgents)
{
	if (showAgents_ != showAgents) {
		showAgents_ = showAgents;
		emit showAgentsChanged(showAgents_);
	}
}

/**
 * TODO
 */
void PsiContactList::setShowHidden(bool showHidden)
{
	if (showHidden_ != showHidden) {
		showHidden_ = showHidden;
		emit showHiddenChanged(showHidden_);
	}
}

/**
 * TODO
 */
void PsiContactList::setShowSelf(bool showSelf)
{
	if (showSelf_ != showSelf) {
		showSelf_ = showSelf;
		emit showSelfChanged(showSelf_);
	}
}

void PsiContactList::setShowOffline(bool showOffline)
{
	if (showOffline_ != showOffline) {
		showOffline_ = showOffline;
		emit showOfflineChanged(showOffline_);
	}
}

void PsiContactList::setContactSortStyle(QString style)
{
	if (contactSortStyle_ != style) {
		contactSortStyle_ = style;
		emit contactSortStyleChanged(contactSortStyle_);
	}
}

PsiAccount *PsiContactList::tryQueueLowestEventId(bool includeDND)
{
	PsiAccount *low = 0;
	int low_id = 0;
	int low_prior = EventPriorityDontCare;

	foreach(PsiAccount *account, enabledAccounts_) {
		int n = account->eventQueue()->nextId();
		if ( n == -1 )
			continue;

		if (!includeDND && account->status().type() == XMPP::Status::DND)
			continue;

		int p = account->eventQueue()->peekNext()->priority();
		if ( !low || (n < low_id && p == low_prior) || p > low_prior ) {
			low = account;
			low_id = n;
			low_prior = p;
		}
	}

	return low;
}

void PsiContactList::accountEnabledChanged()
{
	PsiAccount* account = (PsiAccount*)sender();
	if (account->enabled())
		addEnabledAccount(account);
	else
		removeEnabledAccount(account);
}

PsiAccount* PsiContactList::getAccount(const QString& id) const
{
	foreach(PsiAccount* account, accounts())
		if (account->id() == id)
			return account;

	return 0;
}

PsiAccount* PsiContactList::getAccountByJid(const XMPP::Jid& jid) const
{
	foreach(PsiAccount* account, accounts())
		if (account->jid().compare(jid, false))
			return account;

	return 0;
}

const QList<PsiContact*>& PsiContactList::contacts() const
{
	return contacts_;
}

void PsiContactList::addEnabledAccount(PsiAccount* account)
{
	if (enabledAccounts_.contains(account))
		return;

	enabledAccounts_.append(account);
	connect(account, SIGNAL(addedContact(PsiContact*)), SLOT(accountAddedContact(PsiContact*)));
	connect(account, SIGNAL(removedContact(PsiContact*)), SLOT(accountRemovedContact(PsiContact*)));

	emit beginBulkContactUpdate();
	accountAddedContact(account->selfContact());
	foreach(PsiContact* contact, account->contactList())
		accountAddedContact(contact);
	emit endBulkContactUpdate();
}

void PsiContactList::removeEnabledAccount(PsiAccount* account)
{
	if (!enabledAccounts_.contains(account))
		return;

	emit beginBulkContactUpdate();
	accountRemovedContact(account->selfContact());
	foreach(PsiContact* contact, account->contactList())
		accountRemovedContact(contact);
	emit endBulkContactUpdate();
	disconnect(account, SIGNAL(addedContact(PsiContact*)), this, SLOT(accountAddedContact(PsiContact*)));
	disconnect(account, SIGNAL(removedContact(PsiContact*)), this, SLOT(accountRemovedContact(PsiContact*)));
	enabledAccounts_.removeAll(account);
}

void PsiContactList::accountAddedContact(PsiContact* contact)
{
	Q_ASSERT(!contacts_.contains(contact));
	contacts_.append(contact);
	emit addedContact(contact);
}

void PsiContactList::accountRemovedContact(PsiContact* contact)
{
	Q_ASSERT(contacts_.contains(contact));
	contacts_.removeAll(contact);
	emit removedContact(contact);
}

bool PsiContactList::accountsLoaded() const
{
	return accountsLoaded_;
}
