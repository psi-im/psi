/*
 * capsmanager.h
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

#ifndef CAPSMANAGER_H
#define CAPSMANAGER_H

#include <QMap>
#include <QString>
#include <QObject>

#include "capsspec.h"
#include "xmpp_features.h"

namespace XMPP {
	class Jid;
	class Client;
}
using namespace XMPP;

class CapsManager : public QObject
{
	Q_OBJECT

public:
	CapsManager(XMPP::Client*);

	bool isEnabled();
	void setEnabled(bool);

	void updateCaps(const Jid& jid, const QString& node, const QString& ver, const QString& ext);
	void disableCaps(const Jid& jid);
	bool capsEnabled(const Jid& jid) const;
	XMPP::Features features(const Jid& jid) const;
	QString clientName(const Jid& jid) const;
	QString clientVersion(const Jid& jid) const;
	
signals:
	/**
	 * This signal is emitted when the feature list of a given JID have changed.
	 */
	void capsChanged(const Jid& jid);

protected slots:
	void discoFinished();
	void capsRegistered(const CapsSpec&);

protected:
	void requestDiscoInfo(const Jid& jid, const QString& node);
	
private:
	XMPP::Client* client_;
	bool isEnabled_;
	QMap<QString,CapsSpec> capsSpecs_;
	QMap<CapsSpec,QList<QString> > capsJids_;
};


#endif
