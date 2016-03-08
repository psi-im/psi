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

ServerInfoManager::ServerInfoManager(Client* client) : client_(client)
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
	caps_ = client_->serverCaps();
	if (client_->capsManager()->isEnabled()) {
		// TODO we should really have some easy way to do all this stuff
		if (caps_.isValid()) {
			Jid serverJid(client_->jid().domain());
			client_->capsManager()->updateCaps(serverJid, caps_);
			if (CapsRegistry::instance()->isRegistered(caps_.flatten())) {
				handleReceivedFeatures(client_->capsManager()->disco(serverJid));
			} else {
				connect(CapsRegistry::instance(), SIGNAL(registered(CapsSpec)), SLOT(capsRegistered(CapsSpec)));
			}
		}
	} else {
		JT_DiscoInfo *jt = new JT_DiscoInfo(client_->rootTask());
		connect(jt, SIGNAL(finished()), SLOT(disco_finished()));
		jt->get(client_->jid().domain());
		jt->go(true);
	}
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

void ServerInfoManager::capsRegistered(const CapsSpec &caps)
{
	if (caps_ == caps) {
		handleReceivedFeatures(CapsRegistry::instance()->disco(caps.flatten()));
	}
}

void ServerInfoManager::disco_finished()
{
	JT_DiscoInfo *jt = (JT_DiscoInfo *)sender();
	if (jt->success()) {
		handleReceivedFeatures(jt->item());
	}
}

void ServerInfoManager::handleReceivedFeatures(const DiscoItem &item)
{
	const Features &f = item.features();

	if (f.canMulticast())
		multicastService_ = client_->jid().domain();

	// Identities
	DiscoItem::Identities is = item.identities();
	foreach(DiscoItem::Identity i, is) {
		if (i.category == "pubsub" && i.type == "pep")
			hasPEP_ = true;
	}

	emit featuresChanged();
}
