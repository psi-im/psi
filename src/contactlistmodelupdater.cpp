/*
 * contactlistmodelupdater.cpp - class to group model update operations together
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

#include "contactlistmodelupdater.h"

#include <QTimer>

#include "psicontactlist.h"
#include "psicontact.h"

static const int MAX_COMMIT_DELAY = 30; // in seconds

ContactListModelUpdater::ContactListModelUpdater(PsiContactList* contactList, QObject *parent)
	: QObject(parent)
	, updatesEnabled_(true)
	, contactList_(contactList)
	, commitTimer_(0)
{
	commitTimer_ = new QTimer(this);
	connect(commitTimer_, SIGNAL(timeout()), SLOT(commit()));
	commitTimer_->setSingleShot(true);
	commitTimer_->setInterval(100);

	connect(contactList_, SIGNAL(addedContact(PsiContact*)), SLOT(addContact(PsiContact*)));
	connect(contactList_, SIGNAL(removedContact(PsiContact*)), SLOT(removeContact(PsiContact*)));
	connect(contactList_, SIGNAL(beginBulkContactUpdate()), SLOT(beginBulkUpdate()));
	connect(contactList_, SIGNAL(endBulkContactUpdate()), SLOT(endBulkUpdate()));
}

ContactListModelUpdater::~ContactListModelUpdater()
{
}

void ContactListModelUpdater::clear()
{
	QHashIterator<PsiContact*, bool> it(monitoredContacts_);
	while (it.hasNext()) {
		it.next();
		disconnect(it.key(), 0, this, 0);
	}
	monitoredContacts_.clear();
	operationQueue_.clear();
}

void ContactListModelUpdater::commit()
{
	if (!updatesEnabled())
		return;

// qWarning("updater(%x):commit", (int)this);
	// qWarning("*** ContactListModelUpdater::commit(). operationQueue_.count() = %d", operationQueue_.count());
	commitTimerStartTime_ = QDateTime();
	commitTimer_->stop();
	if (operationQueue_.isEmpty())
		return;

	bool doBulkUpdate = operationQueue_.count() > 1;
	if (doBulkUpdate)
		emit beginBulkContactUpdate();

	QHashIterator<PsiContact*, int> it(operationQueue_);
	while (it.hasNext()) {
		it.next();

		int operations = simplifiedOperationList(it.value());
		if (operations & AddContact)
			emit addedContact(it.key());
		if (operations & RemoveContact)
			Q_ASSERT(false);
		if (operations & UpdateContact)
			emit contactUpdated(it.key());
		if (operations & ContactGroupsChanged)
			emit contactGroupsChanged(it.key());
	}

	if (doBulkUpdate)
		emit endBulkContactUpdate();

	operationQueue_.clear();
}

void ContactListModelUpdater::addContact(PsiContact* contact)
{
	// qWarning(">>> addContact: %s", qPrintable(contact->jid()));
	Q_ASSERT(!monitoredContacts_.contains(contact));
	if (monitoredContacts_.contains(contact))
		return;
	monitoredContacts_[contact] = true;
// qWarning("updater(%x):addContact: %s", (int)this, qPrintable(contact->jid().full()));
	addOperation(contact, AddContact);
	connect(contact, SIGNAL(destroyed(PsiContact*)), SLOT(removeContact(PsiContact*)));
	connect(contact, SIGNAL(updated()), SLOT(contactUpdated()));
	connect(contact, SIGNAL(groupsChanged()), SLOT(contactGroupsChanged()));
	connect(contact, SIGNAL(alert()), SLOT(contactAlert()));
}

/*!
 * removeContact() could be called directly by PsiContactList when account is disabled, or
 * by contact's destructor. We just ensure that the calls are balanced.
 */
void ContactListModelUpdater::removeContact(PsiContact* contact)
{
	// qWarning("<<< removeContact: %s", qPrintable(contact->jid()));
	Q_ASSERT(monitoredContacts_.contains(contact));
	if (!monitoredContacts_.contains(contact))
		return;
	monitoredContacts_.remove(contact);
	disconnect(contact, 0, this, 0);
	emit removedContact(contact);
	operationQueue_.remove(contact);
}

void ContactListModelUpdater::contactAlert()
{
	PsiContact* contact = static_cast<PsiContact*>(sender());
	emit contactAlert(contact);
}

void ContactListModelUpdater::contactUpdated()
{
	PsiContact* contact = static_cast<PsiContact*>(sender());
	Q_ASSERT(monitoredContacts_.contains(contact));
	if (!monitoredContacts_.contains(contact))
		return;
// qWarning("updater(%x):contactUpdated: %s", (int)this, qPrintable(contact->jid().full()));
	addOperation(contact, UpdateContact);
}

void ContactListModelUpdater::contactGroupsChanged()
{
	PsiContact* contact = static_cast<PsiContact*>(sender());
	Q_ASSERT(monitoredContacts_.contains(contact));
	if (!monitoredContacts_.contains(contact))
		return;
// qWarning("updater(%x):contactGroupsChanged: %s", (int)this, qPrintable(contact->jid().full()));
	addOperation(contact, ContactGroupsChanged);
}

void ContactListModelUpdater::beginBulkUpdate()
{
	emit beginBulkContactUpdate();
}

void ContactListModelUpdater::endBulkUpdate()
{
	emit endBulkContactUpdate();
}

void ContactListModelUpdater::addOperation(PsiContact* contact, ContactListModelUpdater::Operation operation)
{
	if (!operationQueue_.contains(contact)) {
		operationQueue_[contact] = operation;
	}
	else {
		operationQueue_[contact] |= operation;
	}

	if (commitTimerStartTime_.isNull())
		commitTimerStartTime_ = QDateTime::currentDateTime();

	if (commitTimerStartTime_.secsTo(QDateTime::currentDateTime()) > MAX_COMMIT_DELAY)
		commit();
	else
		commitTimer_->start();
}

int ContactListModelUpdater::simplifiedOperationList(int operations) const
{
	if (operations & AddContact)
		return AddContact;

	return operations;
}

bool ContactListModelUpdater::updatesEnabled() const
{
	return updatesEnabled_;
}

void ContactListModelUpdater::setUpdatesEnabled(bool updatesEnabled)
{
	if (updatesEnabled_ != updatesEnabled) {
		updatesEnabled_ = updatesEnabled;
		if (updatesEnabled_) {
			commitTimer_->start();
		}
	}
}
