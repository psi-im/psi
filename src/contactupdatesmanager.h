/*
 * contactupdatesmanager.h
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef CONTACTUPDATESMANAGER_H
#define CONTACTUPDATESMANAGER_H

#include <QObject>
#include <QPointer>

class PsiCon;
class QTimer;

#include "psiaccount.h"
#include "xmpp_jid.h"

class ContactUpdatesManager : public QObject
{
	Q_OBJECT
public:
	ContactUpdatesManager(PsiCon* parent);
	~ContactUpdatesManager();

	void contactBlocked(PsiAccount* account, const XMPP::Jid& jid);
	void contactDeauthorized(PsiAccount* account, const XMPP::Jid& jid);
	void contactAuthorized(PsiAccount* account, const XMPP::Jid& jid);
	void contactRemoved(PsiAccount* account, const XMPP::Jid& jid);

private slots:
	void update();

private:
	PsiCon* controller_;
	enum ContactUpdateActionType {
		ContactBlocked = 0,
		ContactAuthorized,
		ContactDeauthorized,
		ContactRemoved
	};
	struct ContactUpdateAction {
		ContactUpdateAction(ContactUpdateActionType _type, PsiAccount* _account, const XMPP::Jid& _jid)
			: type(_type)
			, account(_account)
			, jid(_jid)
		{}
		ContactUpdateActionType type;
		QPointer<PsiAccount> account;
		XMPP::Jid jid;
	};
	QList<ContactUpdateAction> updates_;
	QTimer* updateTimer_;

	void removeAuthRequestEventsFor(PsiAccount* account, const XMPP::Jid& jid, bool denyAuthRequests);
	void removeToastersFor(PsiAccount* account, const XMPP::Jid& jid);
	void removeNotInListContacts(PsiAccount* account, const XMPP::Jid& jid);
};

#endif
