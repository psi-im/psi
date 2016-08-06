/*
 * serverinfomanager.cpp
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

#include "serverinfomanager.h"
#include "xmpp_tasks.h"
#include "xmpp_caps.h"

using namespace XMPP;

ServerInfoManager::ServerInfoManager(Client* client)
	: client_(client)
	, _canMessageCarbons(false)
{
	deinitialize();
	connect(client_, SIGNAL(rosterRequestFinished(bool, int, const QString &)), SLOT(initialize()));
	connect(client_, SIGNAL(disconnected()), SLOT(deinitialize()));
}

void ServerInfoManager::reset()
{
	hasPEP_ = false;
	multicastService_ = QString();
	disconnect(CapsRegistry::instance());
}

void ServerInfoManager::initialize()
{
	JT_DiscoInfo *jt = new JT_DiscoInfo(client_->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(disco_finished()));
	jt->get(client_->jid().domain());
	jt->go(true);
}

void ServerInfoManager::deinitialize()
{
	reset();
	emit featuresChanged();
}

const QString& ServerInfoManager::multicastService() const
{
	return multicastService_;
}

bool ServerInfoManager::hasPEP() const
{
	return hasPEP_;
}

bool ServerInfoManager::canMessageCarbons() const
{
	return _canMessageCarbons;
}

void ServerInfoManager::disco_finished()
{
	JT_DiscoInfo *jt = (JT_DiscoInfo *)sender();
	if (jt->success()) {
		features_ = jt->item().features();

		if (features_.canMulticast())
			multicastService_ = client_->jid().domain();

		_canMessageCarbons = features_.canMessageCarbons();

		// Identities
		DiscoItem::Identities is = jt->item().identities();
		foreach(DiscoItem::Identity i, is) {
			if (i.category == "pubsub" && i.type == "pep")
				hasPEP_ = true;
		}

		emit featuresChanged();
	}
}
