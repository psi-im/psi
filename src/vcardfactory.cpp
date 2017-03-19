/*
 * vcardfactory.cpp - class for caching vCards
 * Copyright (C) 2003  Michail Pishchagin
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

#include <QObject>
#include <QApplication>
#include <QMap>
#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <QDir>

#include "profiles.h"
#include "applicationinfo.h"
#include "vcardfactory.h"
#include "jidutil.h"
#include "psiaccount.h"
#include "xmpp_vcard.h"
#include "xmpp_tasks.h"

/**
 * \brief Factory for retrieving and changing VCards.
 */
VCardFactory::VCardFactory()
	: QObject(qApp), dictSize_(5)
{
}

/**
 * \brief Destroys all cached VCards.
 */
VCardFactory::~VCardFactory()
{
}

/**
 * \brief Returns the VCardFactory instance.
 */
VCardFactory* VCardFactory::instance()
{
	if (!instance_) {
		instance_ = new VCardFactory();
	}
	return instance_;
}


/**
 * Adds a vcard to the cache (and removes other items if necessary)
 */
void VCardFactory::checkLimit(const QString &jid, const VCard &vcard)
{
	if (vcardList_.contains(jid)) {
		vcardList_.removeAll(jid);
		vcardDict_.remove(jid);
	}
	else if (vcardList_.size() > dictSize_) {
		QString j = vcardList_.takeLast();
		vcardDict_.remove(j);
	}

	vcardDict_.insert(jid, vcard);
	vcardList_.push_front(jid);
}


void VCardFactory::taskFinished()
{
	JT_VCard *task = (JT_VCard *)sender();
	bool notifyPhoto = task->property("phntf").toBool();
	if ( task->success() ) {
		Jid j = task->jid();

		saveVCard(j, task->vcard(), notifyPhoto);
	}
}

void VCardFactory::mucTaskFinished()
{
	JT_VCard *task = (JT_VCard *)sender();
	bool notifyPhoto = task->property("phntf").toBool();
	if ( task->success() ) {
		Jid j = task->jid();
		// TODO check for limits. may be like 5 vcards per muc
		mucVcardDict_[j.bare()].insert(j.resource(), task->vcard());

		emit vcardChanged(j);
		if (notifyPhoto && !task->vcard().photo().isEmpty()) {
			emit vcardPhotoAvailable(j, true);
		}
	}
}

void VCardFactory::saveVCard(const Jid& j, const VCard& vcard, bool notifyPhoto)
{
	checkLimit(j.bare(), vcard);

	// save vCard to disk

	// ensure that there's a vcard directory to save into
	QDir p(pathToProfile(activeProfile, ApplicationInfo::CacheLocation));
	QDir v(pathToProfile(activeProfile, ApplicationInfo::CacheLocation) + "/vcard");
	if(!v.exists())
		p.mkdir("vcard");

	QFile file ( ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml" );
	file.open ( QIODevice::WriteOnly );
	QTextStream out ( &file );
	out.setCodec("UTF-8");
	QDomDocument doc;
	doc.appendChild( vcard.toXml ( &doc ) );
	out << doc.toString(4);

	Jid jid = j;
	emit vcardChanged(jid);

	if (notifyPhoto && !vcard.photo().isEmpty()) {
		emit vcardPhotoAvailable(jid, false);
	}
}


/**
 * \brief Call this, when you need a runtime cached vCard.
 */
const VCard VCardFactory::mucVcard(const Jid &j) const
{
	QHash<QString,VCard> d = mucVcardDict_.value(j.bare());
	QHash<QString,VCard>::ConstIterator it = d.constFind(j.resource());
	if (it != d.constEnd()) {
		return *it;
	}
	return VCard();
}

/**
 * \brief Call this, when you need a cached vCard.
 */
VCard VCardFactory::vcard(const Jid &j)
{
	// first, try to get vCard from runtime cache
	if (vcardDict_.contains(j.bare())) {
		return vcardDict_[j.bare()];
	}

	// then try to load from cache on disk
	QFile file ( ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml" );
	file.open (QIODevice::ReadOnly);
	QDomDocument doc;

	if ( doc.setContent(&file, false) ) {
		VCard vcard = VCard::fromXml(doc.documentElement());
		if (!vcard.isNull()) {
			checkLimit(j.bare(), vcard);
			return vcard;
		}
	}

	return VCard();
}


/**
 * \brief Call this when you need to update vCard in cache.
 */
void VCardFactory::setVCard(const Jid &j, const VCard &v, bool notifyPhoto)
{
	saveVCard(j, v, notifyPhoto);
}

/**
 * \brief Updates vCard on specified \a account.
 */
void VCardFactory::setVCard(const PsiAccount* account, const VCard &v, QObject* obj, const char* slot)
{
	JT_VCard* jtVCard_ = new JT_VCard(account->client()->rootTask());
	if (obj)
		connect(jtVCard_, SIGNAL(finished()), obj, slot);
	connect(jtVCard_, SIGNAL(finished()), SLOT(updateVCardFinished()));
	jtVCard_->set(account->jid(), v);
	jtVCard_->go(true);
}

/**
 * \brief Updates vCard on specified \a account.
 */
void VCardFactory::setTargetVCard(const PsiAccount* account, const VCard &v, const Jid &mucJid, QObject* obj, const char* slot)
{
	JT_VCard* jtVCard_ = new JT_VCard(account->client()->rootTask());
	if (obj)
		connect(jtVCard_, SIGNAL(finished()), obj, slot);
	connect(jtVCard_, SIGNAL(finished()), SLOT(updateVCardFinished()));
	jtVCard_->set(mucJid, v, true);
	jtVCard_->go(true);
}

void VCardFactory::updateVCardFinished()
{
	JT_VCard* jtVCard = static_cast<JT_VCard*> (sender());
	if (jtVCard && jtVCard->success()) {
		setVCard(jtVCard->jid(), jtVCard->vcard());
	}
	if (jtVCard) {
		jtVCard->deleteLater();
	}
}

/**
 * \brief Call this when you need to retrieve fresh vCard from server (and store it in cache afterwards)
 */
JT_VCard* VCardFactory::getVCard(const Jid &jid, Task *rootTask, const QObject *obj, const char *slot,
                                 bool cacheVCard, bool isMuc, bool notifyPhoto)
{
	JT_VCard *task = new JT_VCard( rootTask );
	if (notifyPhoto) {
		task->setProperty("phntf", true);
	}
	if ( cacheVCard ) {
		if (isMuc)
			task->connect(task, SIGNAL(finished()), this, SLOT(mucTaskFinished()));
		else
			task->connect(task, SIGNAL(finished()), this, SLOT(taskFinished()));
	}
	task->connect(task, SIGNAL(finished()), obj, slot);
	task->get(Jid(jid.full()));
	task->go(true);
	return task;
}

VCardFactory* VCardFactory::instance_ = NULL;
