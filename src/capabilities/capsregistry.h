/*
 * capsregistry.h
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

#ifndef CAPSREGISTRY_H
#define CAPSREGISTRY_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QPair>

#include "xmpp_features.h"
#include "xmpp_discoitem.h"

#include "capsspec.h"

class QDomDocument;
class QDomElement;

class CapsRegistry : public QObject
{
	Q_OBJECT

public:
	CapsRegistry();

	void registerCaps(const CapsSpec&, const XMPP::DiscoItem::Identities&, const XMPP::Features& features);
	bool isRegistered(const CapsSpec&) const;
	XMPP::Features features(const CapsSpec&) const;
	XMPP::DiscoItem::Identities identities(const CapsSpec&) const;

signals:
	void registered(const CapsSpec&);

public slots:
	void load(QIODevice& target);
	void save(QIODevice& target);

private:
	class CapsInfo
	{
		public:
			CapsInfo();
			const XMPP::Features& features() const;
			const XMPP::DiscoItem::Identities& identities() const;
			
			void setIdentities(const XMPP::DiscoItem::Identities&);
			void setFeatures(const XMPP::Features&);
			
			QDomElement toXml(QDomDocument *) const;
			void fromXml(const QDomElement&);

		protected:
			void updateLastSeen();
			
		private:
			XMPP::Features features_;
			XMPP::DiscoItem::Identities identities_;
			QDateTime lastSeen_;
	};
	QMap<CapsSpec,CapsInfo> capsInfo_;
};


#endif
