/*
 * capsmanager.cpp
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// TODO: 
//  - Fallback on another jid if a disco request should fail. This can be
//    done by keeping a second list of candidate jids to query
//  - Implement Server Optimization support (Section 5).
//  - Implement consistency checking (Section 8).


#include <QString>
#include <QStringList>
#include <QTimer>
#include <QPair>
#include <QtDebug>

#include "capsregistry.h"
#include "capsmanager.h"


//#define REQUEST_TIMEOUT 3000

using namespace XMPP;


/**
 * \class CapsManager
 * \brief A class managing all the capabilities of JIDs and their
 * clients.
 */

/**
 * \brief Default constructor.
 */
CapsManager::CapsManager(const Jid& jid, CapsRegistry* registry, Protocol::DiscoInfoQuerier* discoInfoQuerier) : jid_(jid), registry_(registry), discoInfoQuerier_(discoInfoQuerier), isEnabled_(true)
{
	connect(registry_,SIGNAL(registered(const CapsSpec&)),SLOT(capsRegistered(const CapsSpec&)));
	connect(discoInfoQuerier_, SIGNAL(getDiscoInfo_success(const XMPP::Jid&, const QString&, const XMPP::DiscoItem&)), SLOT(getDiscoInfo_success(const XMPP::Jid&, const QString&, const XMPP::DiscoItem&)));
	connect(discoInfoQuerier_, SIGNAL(getDiscoInfo_error(const XMPP::Jid&, const QString&, int, const QString&)), SLOT(getDiscoInfo_error(const XMPP::Jid&, const QString&, int, const QString&)));
}

CapsManager::~CapsManager()
{
	delete discoInfoQuerier_;
}

/**
 * \brief Checks whether the caps manager is enabled (and does lookups).
 */
bool CapsManager::isEnabled()
{
	return isEnabled_;
}

/**
 * \brief Enables or disables the caps manager.
 */
void CapsManager::setEnabled(bool b)
{
	isEnabled_ = b;
}

/**
 * \brief Registers new incoming capabilities information of a JID.
 * If the features of the entity are unknown, discovery requests are sent to 
 * retrieve the information.
 *
 * @param jid The entity's JID 
 * @param node The entity's caps node
 * @param ver The entity's caps version
 * @param ext The entity's caps extensions
 */
void CapsManager::updateCaps(const Jid& jid, const QString& node, const QString& ver, const QString& ext)
{
	if (jid.compare(jid_,false))
		return;

	CapsSpec c(node,ver,ext);
	CapsSpecs caps = c.flatten();
	if (capsSpecs_[jid.full()] != c) {
		//qDebug() << QString("caps.cpp: Updating caps for %1 (node=%2,ver=%3,ext=%4)").arg(QString(jid.full()).replace('%',"%%")).arg(node).arg(ver).arg(ext);
		
		// Unregister from all old caps nodes
		CapsSpecs old_caps = capsSpecs_[jid.full()].flatten();
		foreach(CapsSpec s, old_caps) {
			if (s != CapsSpec()) {
				capsJids_[s].removeAll(jid.full());
			}
		}

		if (!node.isEmpty() && !ver.isEmpty()) {
			// Register with all new caps nodes
			capsSpecs_[jid.full()] = c;
			foreach(CapsSpec s, caps) {
				if (!capsJids_[s].contains(jid.full())) {
					capsJids_[s].push_back(jid.full());
				}
			}
			
			emit capsChanged(jid); 

			// Register new caps and check if we need to discover features
			if (isEnabled()) {
				foreach(CapsSpec s, caps) {
					if (!registry_->isRegistered(s) && capsJids_[s].count() == 1) {
						//qDebug() << QString("caps.cpp: Sending disco request to %1, node=%2").arg(QString(jid.full()).replace('%',"%%")).arg(node + "#" + s.extensions());
						discoInfoQuerier_->getDiscoInfo(jid,node + '#' + s.extensions());
					}
				}
			}
		}
		else {
			// Remove all caps specifications
			qWarning() << QString("caps.cpp: Illegal caps info from %1: node=%2, ver=%3").arg(QString(jid.full()).replace('%',"%%")).arg(node).arg(ver);
			capsSpecs_.remove(jid.full());
		}
	}
	else {
		// Add to the list of jids
		foreach(CapsSpec s, caps) {
			capsJids_[s].push_back(jid.full());
		}
	}
}

/**
 * \brief Removes all feature information for a given JID.
 *
 * @param jid The entity's JID 
 */
void CapsManager::disableCaps(const Jid& jid)
{
	//qDebug() << QString("caps.cpp: Disabling caps for %1.").arg(QString(jid.full()).replace('%',"%%"));
	if (capsEnabled(jid)) {
		CapsSpecs cs = capsSpecs_[jid.full()].flatten();
		foreach(CapsSpec s, cs) {
			if (s != CapsSpec()) {
				capsJids_[s].removeAll(jid.full());
			}
		}
		capsSpecs_.remove(jid.full());
		emit capsChanged(jid);
	}
}

/**
 * \brief Called when a reply to disco#info request was received.
 * If the result was succesful, the resulting features are recorded in the
 * features database for the requested node, and all the affected jids are
 * put in the queue for update notification.
 */
void CapsManager::getDiscoInfo_success(const XMPP::Jid& jid, const QString& node, const XMPP::DiscoItem& item)
{
	//qDebug() << QString("caps.cpp: Disco response from %1, node=%2").arg(QString(jid.full()).replace('%',"%%")).arg(node);
	// Update features
	bool ok = false;
	CapsSpec cs(getCapsSpecForNode(jid, node, ok));
	if (!ok) {
		return;
	}
	registry_->registerCaps(cs,item.identities(),item.features().list());
}

void CapsManager::getDiscoInfo_error(const XMPP::Jid& jid, const QString& node, int, const QString&)
{
	qWarning() << QString("capsmanager.cpp: Disco to '%1' at node '%2' failed.").arg(jid.full()).arg(node);
}


CapsSpec CapsManager::getCapsSpecForNode(const XMPP::Jid& jid, const QString& disco_node, bool& ok) const
{
    int hash_index = disco_node.indexOf('#');
    if (hash_index == -1) {
		qWarning() << "CapsManager: Node" << disco_node << "invalid";
		ok = false;
		return CapsSpec();
    }
	QString node = disco_node.left(hash_index);
	QString ext = disco_node.right(disco_node.length() - hash_index - 1);
	CapsSpec jid_cs = capsSpecs_[jid.full()];
	if (jid_cs.node() == node) {
		ok = true;
		return CapsSpec(node,jid_cs.version(),ext);
	}
	else {
		ok = false;
		return CapsSpec();
	}
}

/**
 * \brief This slot is called whenever capabilities of a client were discovered.
 * All jids with the corresponding client are updated.
 */
void CapsManager::capsRegistered(const CapsSpec& cs) 
{
	// Notify affected jids.
	foreach(QString s, capsJids_[cs]) {
		//qDebug() << QString("caps.cpp: Notifying %1.").arg(s.replace('%',"%%"));
		emit capsChanged(s);
	}
}

/**
 * \brief Checks whether a given JID is broadcastingn its entity capabilities.
 */
bool CapsManager::capsEnabled(const Jid& jid) const
{
	return capsSpecs_.contains(jid.full());
}


/**
 * \brief Requests the list of features of a given JID.
 */
XMPP::Features CapsManager::features(const Jid& jid) const
{
	//qDebug() << "caps.cpp: Retrieving features of " << jid.full();
	QStringList f;
	if (capsEnabled(jid)) {
		CapsSpecs cs = capsSpecs_[jid.full()].flatten();
		foreach(CapsSpec s, cs) {
			//qDebug() << QString("    %1").arg(registry_->features(s).list().join("\n"));
			f += registry_->features(s).list();
		}
	}
	return Features(f);
}
	
/**
 * \brief Returns the client name of a given jid.
 * \param jid the jid to retrieve the client name of
 */
QString CapsManager::clientName(const Jid& jid) const
{
	if (capsEnabled(jid)) {
		QString name;
		CapsSpec cs = capsSpecs_[jid.full()];
		const DiscoItem::Identities& i = registry_->identities(CapsSpec(cs.node(),cs.version(),cs.version()));
		if (i.count() > 0) {
			name = i.first().name;
		}
		
		// Try to be intelligent about the name
		if (name.isEmpty()) {
			name = cs.node();
			if (name.startsWith("http://"))
				name = name.right(name.length() - 7);
				
			if (name.startsWith("www."))
				name = name.right(name.length() - 4);
			
			int cut_pos = name.indexOf("/");
			if (cut_pos != -1)
				name = name.left(cut_pos);
		}

		return name;
	}
	else {
		return QString();
	}
}

/**
 * \brief Returns the client version of a given jid.
 */
QString CapsManager::clientVersion(const Jid& jid) const
{
	return (capsEnabled(jid) ? capsSpecs_[jid.full()].version() : QString());
}
