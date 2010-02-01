/*
 * contactupdatesmanager.cpp
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

#include "contactupdatesmanager.h"

#include <QTimer>

#include "psiaccount.h"
#include "psicontact.h"
#include "psicon.h"
#include "psievent.h"
#include "userlist.h"

#ifdef YAPSI_ACTIVEX_SERVER
#include "yaonline.h"
#endif

ContactUpdatesManager::ContactUpdatesManager(PsiCon* parent)
	: QObject(parent)
	, controller_(parent)
{
	Q_ASSERT(controller_);
	updateTimer_ = new QTimer(this);
	updateTimer_->setSingleShot(false);
	updateTimer_->setInterval(0);
	connect(updateTimer_, SIGNAL(timeout()), SLOT(update()));
}

ContactUpdatesManager::~ContactUpdatesManager()
{
}

void ContactUpdatesManager::contactBlocked(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	updates_ << ContactUpdateAction(ContactBlocked, account, jid);
	updateTimer_->start();
}

void ContactUpdatesManager::contactDeauthorized(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	updates_ << ContactUpdateAction(ContactDeauthorized, account, jid);
	updateTimer_->start();
}

void ContactUpdatesManager::contactAuthorized(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	updates_ << ContactUpdateAction(ContactAuthorized, account, jid);
	updateTimer_->start();
}

void ContactUpdatesManager::contactRemoved(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	// we must act immediately, since otherwise all corresponding events
	// will be simply deleted
	removeAuthRequestEventsFor(account, jid, true);
	removeToastersFor(account, jid);
}

void ContactUpdatesManager::removeAuthRequestEventsFor(PsiAccount* account, const XMPP::Jid& jid, bool denyAuthRequests)
{
	Q_ASSERT(account);
	if (!account || !controller_)
		return;

	foreach(EventQueue::PsiEventId p, account->eventQueue()->eventsFor(jid, false)) {
		PsiEvent* e = p.second;
		if (e->type() == PsiEvent::Auth) {
			AuthEvent* authEvent = static_cast<AuthEvent*>(e);
			if (authEvent->authType() == "subscribe") {
				if (denyAuthRequests) {
					account->dj_deny(jid);
				}
#ifdef YAPSI_ACTIVEX_SERVER
				controller_->yaOnline()->closeNotify(p.first, e);
#endif
				account->eventQueue()->dequeue(e);
				e->deleteLater();
			}
		}
	}
}

void ContactUpdatesManager::removeToastersFor(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	if (!account || !controller_)
		return;

	foreach(EventQueue::PsiEventId p, account->eventQueue()->eventsFor(jid, false)) {
		PsiEvent* e = p.second;
		if (e->type() == PsiEvent::Message
#ifdef YAPSI
		    || e->type() == PsiEvent::Mood
#endif
		    )
		{
#ifdef YAPSI_ACTIVEX_SERVER
			controller_->yaOnline()->closeNotify(p.first, e);
#endif
			account->eventQueue()->dequeue(e);
			e->deleteLater();
		}
	}
}

void ContactUpdatesManager::removeNotInListContacts(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	if (!account)
		return;

	foreach(UserListItem* u, account->findRelevant(jid)) {
		if (u && !u->inList()) {
			account->actionRemove(u->jid());
		}
	}
}

void ContactUpdatesManager::update()
{
	while (!updates_.isEmpty()) {
		ContactUpdateAction action = updates_.takeFirst();
		if (!action.account)
			continue;

		if (action.type == ContactBlocked) {
			removeAuthRequestEventsFor(action.account, action.jid, true);
			removeNotInListContacts(action.account, action.jid);
		}
		else if (action.type == ContactAuthorized) {
			removeAuthRequestEventsFor(action.account, action.jid, false);
		}
		else if (action.type == ContactDeauthorized) {
			removeAuthRequestEventsFor(action.account, action.jid, false);
			removeNotInListContacts(action.account, action.jid);
		}
		else if (action.type == ContactRemoved) {
			Q_ASSERT(false);
		}
	}

	if (updates_.isEmpty())
		updateTimer_->stop();
	else
		updateTimer_->start();
}
