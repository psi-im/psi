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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psicontactlist.h"
#include "psiaccount.h"
#include "psievent.h"
#include "accountadddlg.h"
#include "serverinfomanager.h"
#include "psicon.h"

/**
 * Constructs new PsiContactList. \param psi will not be PsiContactList's parent though.
 */
PsiContactList::PsiContactList(PsiCon* psi) : ContactList(), psi_(psi)
{
}

/**
 * Destroys the PsiContactList along with all PsiAccounts.
 */
PsiContactList::~PsiContactList()
{
	// PsiAccount calls some signals while being deleted prior to being unlinked,
	// which in result could cause calls to PsiContactList::accounts()
	QList<PsiAccount*> toDelete(accounts_);
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

/**
 * Returns true if enabledAccounts() list is not empty.
 */
bool PsiContactList::haveEnabledAccounts() const
{
	return !enabledAccounts_.isEmpty();
}

/**
 * At the moment, it returns first enabled account.
 */
PsiAccount *PsiContactList::defaultAccount() const
{
	return enabledAccounts_.first();
}

/**
 * Creates new PsiAccount based on some initial settings. This is used by AccountAddDlg.
 */
void PsiContactList::createAccount(const QString& name, const Jid& j, const QString& pass, bool opt_host, const QString& host, int port, bool legacy_ssl_probe, UserAccount::SSLFlag ssl, int proxy, bool modify)
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
	acc.proxy_index = proxy;
	acc.legacy_ssl_probe = legacy_ssl_probe;

	PsiAccount *pa = loadAccount(acc);
	emit saveAccounts();

	// pop up the modify dialog so the user can customize the new account
	if (modify) 
		pa->modify();
}

void PsiContactList::createAccount(const UserAccount& acc)
{
	loadAccount(acc);
	emit saveAccounts();
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
	PsiAccount *pa = psi_->createAccount(acc);
	connect(pa, SIGNAL(enabledChanged()), SIGNAL(accountCountChanged()));
	emit accountAdded(pa);
	return pa;
}

/**
 * Loads accounts from \param list
 */
void PsiContactList::loadAccounts(const UserAccountList &list)
{
	foreach(UserAccount account, list)
		loadAccount(account);
}

/**
 * Creates new UserAccountList from all the PsiAccounts ready for saving to disk.
 */
UserAccountList PsiContactList::getUserAccountList() const
{
	UserAccountList acc;
	foreach(PsiAccount* account, accounts_)
		acc += account->userAccount();

	return acc;
}

/**
 * It's called by each and every PsiAccount on its creation.
 */
void PsiContactList::link(PsiAccount* account)
{
	Q_ASSERT(!accounts_.contains(account));
	connect(account, SIGNAL(updatedActivity()), this, SIGNAL(accountActivityChanged()));
	connect(account->serverInfoManager(),SIGNAL(featuresChanged()), this, SIGNAL(accountFeaturesChanged()));
	accounts_.append(account);
	if (account->enabled())
		enabledAccounts_.append(account);
	connect(account, SIGNAL(enabledChanged()), SLOT(accountEnabledChanged()));
	emit accountCountChanged();
}

/**
 * It's called by each and every PsiAccount on its destruction.
 */
void PsiContactList::unlink(PsiAccount* account)
{
	Q_ASSERT(accounts_.contains(account));
	disconnect(account, SIGNAL(updatedActivity()), this, SIGNAL(accountActivityChanged()));
	accounts_.removeAll(account);
	enabledAccounts_.removeAll(account);
	emit accountCountChanged();
}


PsiAccount *PsiContactList::tryQueueLowestEventId(bool includeDND)
{
	PsiAccount *low = 0;
	int low_id = 0;
	int low_prior = option.EventPriorityDontCare;

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
	enabledAccounts_.removeAll(account);
	if (account->enabled())
		enabledAccounts_.append(account);
}

