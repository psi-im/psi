/*
 * capsregistry.cpp
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

#include <QApplication>
#include <QDebug>

#include "im.h"
#include "psiaccount.h"
#include "capsregistry.h"

using namespace XMPP;

// -----------------------------------------------------------------------------

CapsRegistry::CapsInfo::CapsInfo()
{
	updateLastSeen();
}

const XMPP::Features& CapsRegistry::CapsInfo::features() const
{
	return features_;
}

const DiscoItem::Identities& CapsRegistry::CapsInfo::identities() const
{
	return identities_;
}

void CapsRegistry::CapsInfo::setIdentities(const DiscoItem::Identities& i)
{
	identities_ = i;
}

void CapsRegistry::CapsInfo::setFeatures(const XMPP::Features& f)
{
	features_ = f;
}
	
void CapsRegistry::CapsInfo::updateLastSeen()
{
	lastSeen_ = QDateTime::currentDateTime();
}

QDomElement CapsRegistry::CapsInfo::toXml(QDomDocument *doc) const
{
	QDomElement info = doc->createElement("info");
	info.setAttribute("last-seen",lastSeen_.toString(Qt::ISODate));

	// Identities
	for (DiscoItem::Identities::ConstIterator i = identities_.begin(); i != identities_.end(); ++i) {
		QDomElement identity = doc->createElement("identity");
		identity.setAttribute("category",(*i).category);
		identity.setAttribute("name",(*i).name);
		identity.setAttribute("type",(*i).type);
		info.appendChild(identity);
	}

	// Features
	foreach(QString f, features_.list()) {
		QDomElement feature = doc->createElement("feature");
		feature.setAttribute("node",f);
		info.appendChild(feature);
	}

	return info;
}

void CapsRegistry::CapsInfo::fromXml(const QDomElement& e)
{
	if (e.tagName() != "info") {
		qWarning("caps.cpp: Invalid info element");
		return;
	}
	
	if (!e.attribute("last-seen").isEmpty()) {
		QDateTime last = QDateTime::fromString(e.attribute("last-seen"),Qt::ISODate);
		if (last.isValid())
			lastSeen_ = last;
		else
			qWarning("Invalid date in caps registry");
	}

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull()) {
			qWarning("caps.cpp: Null element");
			continue;
		}

		if(i.tagName() == "identity") {
			DiscoItem::Identity id;
			id.category = i.attribute("category");
			id.name = i.attribute("name");
			id.type = i.attribute("type");
			identities_ += id;
		}
		else if (i.tagName() == "feature") {
			features_.addFeature(i.attribute("node"));
		}
		else {
			qWarning("caps.cpp: Unknown element");
		}
	}
}

// -----------------------------------------------------------------------------
/**
 * \class CapsRegistry
 * \brief A singleton class managing the capabilities of clients.
 */

/**
 * \brief Default constructor.
 */
CapsRegistry::CapsRegistry() : QObject(qApp)
{
	connect(QCoreApplication::instance(),SIGNAL(aboutToQuit()),SLOT(save()));
}

/**
 * \brief Destructor. Saves data before exiting.
 */
CapsRegistry::~CapsRegistry()
{
	save();
}


/**
 * \brief Convert all capabilities info to XML.
 */
void CapsRegistry::save()
{
	//qDebug() << QString("Saving caps to '%1'").arg(fileName_);
	
	if (fileName_.isEmpty())
		return;

	// Generate XML
	QDomDocument doc;
	QDomElement capabilities = doc.createElement("capabilities");
	doc.appendChild(capabilities);
	QMap<CapsSpec,CapsInfo>::ConstIterator i = capsInfo_.begin();
	for( ; i != capsInfo_.end(); i++) {
		QDomElement info = i.data().toXml(&doc);
		info.setAttribute("node",i.key().node());
		info.setAttribute("ver",i.key().version());
		info.setAttribute("ext",i.key().extensions());
		capabilities.appendChild(info);
	}

	// Save
	QFile f(fileName_);
	if(!f.open(IO_WriteOnly)) {
		qWarning("caps.cpp: Unable to open caps file");
		return;
	}
	QTextStream t;
	t.setDevice(&f);
	t.setEncoding(QTextStream::UnicodeUTF8);
	t << doc.toString();
	f.close();
}

/**
 * \brief Sets the file to save the capabilities info to
 */
void CapsRegistry::setFile(const QString& fileName)
{
	fileName_ = fileName;
	
	// Load settings
	//qDebug() << "Loading " << fileName;
	QDomDocument doc;
	QFile f(fileName_);
	if (!f.open(IO_ReadOnly))
		return;
	if (!doc.setContent(&f))
		return;
	f.close();

	QDomElement caps = doc.documentElement();
	if (caps.tagName() != "capabilities") {
		qWarning("caps.cpp: Invalid capabilities element");
		return;
	}
		
	for(QDomNode n = caps.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull()) {
			qWarning("capsregistry.cpp: Null element");
			continue;
		}

		if(i.tagName() == "info") {
			CapsInfo info;
			info.fromXml(i);
			CapsSpec spec(i.attribute("node"),i.attribute("ver"),i.attribute("ext"));
			capsInfo_[spec] = info;
			//qDebug() << QString("Read %1 %2 %3").arg(spec.node()).arg(spec.version()).arg(spec.extensions());
		}
		else {
			qWarning("capsregistry.cpp: Unknown element");
		}
	}
}

/**
 * \brief Registers capabilities of a client.
 */
void CapsRegistry::registerCaps(const CapsSpec& spec,const XMPP::DiscoItem::Identities& identities,const XMPP::Features& features)
{
	if (!isRegistered(spec)) {
		CapsInfo info;
		info.setIdentities(identities);
		info.setFeatures(features);
		capsInfo_[spec] = info;
		save();
		emit registered(spec);
	}
}

/**
 * \brief Checks if capabilities have been registered.
 */
bool CapsRegistry::isRegistered(const CapsSpec& spec) const
{
	return capsInfo_.contains(spec);
}


/**
 * \brief Retrieves the features of a given caps spec.
 */
XMPP::Features CapsRegistry::features(const CapsSpec& spec) const
{
	return capsInfo_[spec].features();
}

/**
 * \brief Retrieves the identities of a given caps spec.
 */
XMPP::DiscoItem::Identities CapsRegistry::identities(const CapsSpec& spec) const
{
	return capsInfo_[spec].identities();
}

/**
 * \brief Retrieves the instance of the CapsRegistry.
 * If no instance existed, a new one is created.
 */
CapsRegistry* CapsRegistry::instance()
{
	if (!instance_) {
		instance_ = new CapsRegistry();
	}
	return instance_;
}

/**
 * \brief The instance of the CapsRegistry.
 */
CapsRegistry* CapsRegistry::instance_ = NULL;

