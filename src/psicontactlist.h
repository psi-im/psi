/*
 * psicontactlist.h - general abstraction of the psi-specific contact list
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

#ifndef PSICONTACTLIST_H
#define PSICONTACTLIST_H

#include <QList>

#include "profiles.h"

using namespace XMPP;

class PsiCon;
class PsiAccount;
namespace XMPP { class Jid; }

class PsiContactList : public QObject
{
	Q_OBJECT
public:
	PsiContactList(PsiCon* psi);
	~PsiContactList();

	PsiCon* psi() const;

	const QList<PsiAccount*>& accounts() const;
	const QList<PsiAccount*>& enabledAccounts() const;
	bool haveActiveAccounts() const;
	bool haveEnabledAccounts() const;

	PsiAccount *defaultAccount() const;

	UserAccountList getUserAccountList() const;

	void createAccount(const QString& name, const Jid& j = "", const QString& pass = "", bool opt_host = false, const QString& host = "", int port = 5222, bool legacy_ssl_probe = true, UserAccount::SSLFlag ssl = UserAccount::SSL_Auto, QString proxyID = "", const QString &tlsOverrideDomain="", const QByteArray &tlsOverrideCert=QByteArray(), bool modify = true);
	void createAccount(const UserAccount&);
	void removeAccount(PsiAccount*);
	void setAccountEnabled(PsiAccount*, bool enabled = TRUE);

	int queueCount() const;
	PsiAccount *queueLowestEventId();

	void loadAccounts(const UserAccountList &);
	void link(PsiAccount*);
	void unlink(PsiAccount*);

	void beginBulkOperation();
	void endBulkOperation();

signals:
	/**
	 * This signal is emitted when account is loaded from disk or created
	 * anew.
	 */
	void accountAdded(PsiAccount*);
	/**
	 * This signal is emitted when account is completely removed from the
	 * program, i.e. deleted.
	 */
	void accountRemoved(PsiAccount*);
	/**
	 * This signal is emitted when number of accounts managed by
	 * PsiContactList changes.
	 */
	void accountCountChanged();
	/**
	 * This signal is emitted when one of the accounts emits
	 * activityChanged() signal.
	 */
	void accountActivityChanged();
	/**
	 * This signal is emitted when the features of one of the accounts change.
	 */
	void accountFeaturesChanged();
	/**
	 * This signal is emitted when either new account was added, or 
	 * existing one was removed altogether.
	 */
	void saveAccounts();

private slots:
	void accountEnabledChanged();

private:
	PsiAccount *loadAccount(const UserAccount &);
	PsiAccount *tryQueueLowestEventId(bool includeDND);

	PsiCon *psi_;
	PsiContactList *contactList_;
	QList<PsiAccount *> accounts_, enabledAccounts_;
};

#endif
