/*
 * privacymanager.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef PSIPRIVACYMANAGER_H
#define PSIPRIVACYMANAGER_H

#include <QObject>
#include <QStringList>
#include <QHash>

#include "privacymanager.h"

#include "xmpp_jid.h"

class QString;
class PsiAccount;
class PrivacyList;
class PrivacyListItem;
class PrivacyListListener;
namespace XMPP {
	class Task;
}


class PsiPrivacyManager : public PrivacyManager
{
	Q_OBJECT

public:
	PsiPrivacyManager(PsiAccount* account, XMPP::Task* rootTask);
	virtual ~PsiPrivacyManager();

	void requestListNames();

	void changeDefaultList(const QString& name);
	void changeActiveList(const QString& name);
	void changeList(const PrivacyList& list);
	void getDefaultList();
	void requestList(const QString& name);

	// Convenience
	void block(const QString&);

protected:
	static QStringList blockedContacts(const PrivacyList&, bool* allBlocked);

// Can these be private ?
protected slots:
	void receiveLists();
	void receiveList();
	void changeDefaultList_finished();
	void changeActiveList_finished();
	void changeList_finished();
	void getDefault_listsReceived(const QString&, const QString&, const QStringList&);
	void getDefault_listsError();
	void getDefault_listReceived(const PrivacyList&);
	void getDefault_listError();

	void block_getDefaultList_success(const PrivacyList&);
	void block_getDefaultList_error();

private:
	XMPP::Task* rootTask_;
	PrivacyListListener* listener_;

	bool getDefault_waiting_;
	QString getDefault_default_;

	QStringList block_targets_;
	bool block_waiting_;

public:
	bool isAvailable() const;

	bool isContactBlocked(const XMPP::Jid& jid) const;
	void setContactBlocked(const XMPP::Jid& jid, bool blocked);

signals:
	void availabilityChanged();
	void listChanged(const QStringList& contacts);

	//void simulateContactOffline(const XMPP::Jid& contact);

private slots:
	void newListReceived(const PrivacyList& p);
	void newListsReceived(const QString& defaultList, const QString& activeList, const QStringList& lists);
	void newListsError();

	void accountStateChanged();

	void newChangeDefaultList_success();
	void newChangeDefaultList_error();
	void newChangeActiveList_success();
	void newChangeActiveList_error();

private slots:
	void privacyListChanged(const QString& name);

private:
	PsiAccount* account_;
	bool accountAvailable_;
	bool isAvailable_;
	QHash<QString, PrivacyList*> lists_;
	QHash<QString, bool> isBlocked_;

	void invalidateBlockedListCache();
	void setIsAvailable(bool available);

	void createBlockedList();
	PrivacyList* blockedList() const;
	PrivacyListItem blockItemFor(const XMPP::Jid& jid) const;

	QStringList blockedContacts() const;

	QString blockedListName_, tmpActiveListName_;
	bool isAuthorized(const XMPP::Jid& jid) const;
};

#endif
