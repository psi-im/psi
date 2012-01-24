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

#include "privacymanager.h"

class QString;
class PrivacyList;
class PrivacyListListener;
namespace XMPP {
	class Task;
}


class PsiPrivacyManager : public PrivacyManager
{
	Q_OBJECT

public:
	PsiPrivacyManager(XMPP::Task* rootTask);
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
};

#endif
