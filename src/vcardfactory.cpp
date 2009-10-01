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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
	foreach (VCard *vcard, vcardDict_)
		delete vcard;
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
void VCardFactory::checkLimit(QString jid, VCard *vcard)
{
	if (vcardList_.contains(jid)) {
		vcardList_.removeAll(jid);
		delete vcardDict_.take(jid);
	}
	else if (vcardList_.size() > dictSize_) {
		QString j = vcardList_.takeLast();
		delete vcardDict_.take(j);
	}

	vcardDict_[jid] = vcard;
	vcardList_.push_front(jid);
}


void VCardFactory::taskFinished()
{
	JT_VCard *task = (JT_VCard *)sender();
	if ( task->success() ) {
		Jid j = task->jid();

		saveVCard(j, task->vcard());
	}
}

void VCardFactory::saveVCard(const Jid& j, const VCard& _vcard)
{
	VCard *vcard = new VCard;
	*vcard = _vcard;
	checkLimit(j.bare(), vcard);

	// save vCard to disk

	// ensure that there's a vcard directory to save into
	QDir p(pathToProfile(activeProfile));
	QDir v(pathToProfile(activeProfile) + "/vcard");
	if(!v.exists())
		p.mkdir("vcard");

	QFile file ( ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml" );
	file.open ( QIODevice::WriteOnly );
	QTextStream out ( &file );
	out.setCodec("UTF-8");
	QDomDocument doc;
	doc.appendChild( vcard->toXml ( &doc ) );
	out << doc.toString(4);

	Jid jid = j;
	emit vcardChanged(jid);
}

/**
 * \brief Call this, when you need a cached vCard.
 */
const VCard* VCardFactory::vcard(const Jid &j)
{
	// first, try to get vCard from runtime cache
	if (vcardDict_.contains(j.bare())) {
		return vcardDict_[j.bare()];
	}
	
	// then try to load from cache on disk
	QFile file ( ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml" );
	file.open (QIODevice::ReadOnly);
	QDomDocument doc;
	VCard *vcard = new VCard;
	if ( doc.setContent(&file, false) ) {
		vcard->fromXml( doc.documentElement() );
		checkLimit(j.bare(), vcard);
		return vcard;
	}

	delete vcard;
	return 0;
}


/**
 * \brief Call this when you need to update vCard in cache.
 */
void VCardFactory::setVCard(const Jid &j, const VCard &v)
{
	saveVCard(j, v);
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
JT_VCard* VCardFactory::getVCard(const Jid &jid, Task *rootTask, const QObject *obj, const char *slot, bool cacheVCard)
{
	JT_VCard *task = new JT_VCard( rootTask );
	if ( cacheVCard )
		task->connect(task, SIGNAL(finished()), this, SLOT(taskFinished()));
	task->connect(task, SIGNAL(finished()), obj, slot);
	task->get(Jid(jid.full()));
	task->go(true);
	return task;
}
	
VCardFactory* VCardFactory::instance_ = NULL;
