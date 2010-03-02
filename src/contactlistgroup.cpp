/*
 * contactlistgroup.cpp - flat contact list group class
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

#include "contactlistgroup.h"

#include <QTimer>

#include "contactlistitemproxy.h"
#include "contactlistgroupmenu.h"
#include "contactlistmodel.h"
#include "psicontact.h"
#include "contactlistgroupstate.h"
#include "contactlistgroupcache.h"
#ifdef YAPSI
#include "fakegroupcontact.h"
#endif

static QString GROUP_DELIMITER = "::";

/**
 * Flat group class.
 */
ContactListGroup::ContactListGroup(ContactListModel* model, ContactListGroup* parent)
	: ContactListItem()
	, model_(model)
	, parent_(parent)
	, updateOnlineContactsTimer_(0)
	, haveOnlineContacts_(false)
	, onlineContactsCount_(0)
	, totalContactsCount_(0)
{
	updateOnlineContactsTimer_ = new QTimer(this);
	connect(updateOnlineContactsTimer_, SIGNAL(timeout()), SLOT(updateOnlineContactsFlag()));
	updateOnlineContactsTimer_->setSingleShot(true);
	updateOnlineContactsTimer_->setInterval(100);
}

ContactListGroup::~ContactListGroup()
{
	clearGroup();
}

void ContactListGroup::clearGroup()
{
	foreach(PsiContact* contact, contacts_)
		removeContact(contact);
	qDeleteAll(contacts_);
	contacts_.clear();
}

/**
 * Used only in fullName().
 */
QString ContactListGroup::internalGroupName() const
{
	return name();
}

QString ContactListGroup::fullName() const
{
	QStringList name;
	const ContactListGroup* group = this;
	while (group) {
		if (!group->internalGroupName().isEmpty())
			name.prepend(group->internalGroupName());
		group = group->parent();
	}
	return name.join(groupDelimiter());
}

const QString& ContactListGroup::groupDelimiter()
{
	return GROUP_DELIMITER;
}

void ContactListGroup::setGroupDelimiter(const QString& str)
{
	GROUP_DELIMITER = str;
}

QString ContactListGroup::sanitizeGroupName(const QString& name) const
{
	return name.split(groupDelimiter(), QString::SkipEmptyParts).join(groupDelimiter());
}

QStringList ContactListGroup::sanitizeGroupNames(const QStringList& names) const
{
	QStringList sanitized;
	foreach(QString name, names) {
		sanitized.append(sanitizeGroupName(name));
	}
	return sanitized;
}

ContactListModel::Type ContactListGroup::type() const
{
	return ContactListModel::GroupType;
}

const QString& ContactListGroup::name() const
{
	return name_;
}

void ContactListGroup::setName(const QString& name)
{
	model()->renameGroup(this, name);
}

/**
 * Renames the group without telling the model.
 */
void ContactListGroup::quietSetName(const QString& name)
{
	if (!name_.isNull())
		model_->groupCache()->removeGroup(this);

	name_ = name;

	if (!name_.isNull())
		model_->groupCache()->addGroup(this);
}

bool ContactListGroup::isExpandable() const
{
	return true;
}

bool ContactListGroup::expanded() const
{
	return model()->groupState()->groupExpanded(this);
}

void ContactListGroup::setExpanded(bool expanded)
{
	model()->groupState()->setGroupExpanded(this, expanded);
}

bool ContactListGroup::isEditable() const
{
	bool result = false;
	foreach(ContactListItemProxy* proxy, items_) {
		if (proxy->item()) {
			if (proxy->item()->isEditable()) {
				result = true;
				break;
			}
		}
	}

	return result && model()->contactList()->haveAvailableAccounts();
}

bool ContactListGroup::isRemovable() const
{
	bool result = false;
	foreach(ContactListItemProxy* proxy, items_) {
		if (proxy->item()) {
			if (proxy->item()->isRemovable()) {
				result = true;
				break;
			}
		}
	}

	return result && isEditable();
}

ContactListItemMenu* ContactListGroup::contextMenu(ContactListModel* model)
{
	return new ContactListGroupMenu(this, model);
}

void ContactListGroup::addItem(ContactListItemProxy* item)
{
	Q_ASSERT(!items_.contains(item));
	int index = items_.count();
	model_->itemAboutToBeInserted(this, index);
	items_.append(item);
	model_->insertedItem(this, index);
	updateOnlineContactsTimer_->start();
}

void ContactListGroup::removeItem(ContactListItemProxy* item)
{
	int index = items_.indexOf(item);
	Q_ASSERT(index != -1);
	model_->itemAboutToBeRemoved(this, index);
	items_.remove(index);
	delete item;
	model_->removedItem(this, index);
	updateOnlineContactsTimer_->start();
}

/**
 * contactGroups handling rules:
 * 1. List is empty: we must not add this contact to self;
 * 2. List contains null QString() element: we must add contact to General group
 * 3. List contains non-null QString() elements: those contain group names (with group separators)
 */
void ContactListGroup::addContact(PsiContact* contact, QStringList contactGroups)
{
	if (contactGroups.isEmpty())
		return;

	if (findContact(contact))
		return;
	Q_ASSERT(!contacts_.contains(contact));
CL_DEBUG("ContactListGroup(%x)::addContact: %s (items = %d, contacts = %d)", this, qPrintable(contact->jid().full()), items_.count(), contacts_.count());
	contacts_.append(contact);
	addItem(new ContactListItemProxy(this, contact));

	model_->groupCache()->addContact(this, contact);
}

void ContactListGroup::removeContact(PsiContact* contact)
{
	int index = contacts_.indexOf(contact);
	Q_ASSERT(index != -1);
CL_DEBUG("ContactListGroup(%x)::removeContact: %s (items = %d, contacts = %d)", this, qPrintable(contact->jid().full()), items_.count(), contacts_.count());
	removeItem(findContact(contact));
	contacts_.remove(index);

	model_->groupCache()->removeContact(this, contact);
}

// Some room for future optimizations here
ContactListItemProxy* ContactListGroup::findContact(PsiContact* contact) const
{
	foreach(ContactListItemProxy* item, items_)
		if (item->item() == contact)
			return item;
	return 0;
}

ContactListItemProxy* ContactListGroup::findGroup(ContactListGroup* group) const
{
	foreach(ContactListItemProxy* item, items_)
		if (item->item() == group)
			return item;
	return 0;
}

// ContactListItemProxy* ContactListGroup::findAccount(ContactListAccountGroup* account) const
// {
// 	foreach(ContactListItemProxy* item, items_)
// 		if (item->item() == account)
// 			return item;
// 	return 0;
// }

void ContactListGroup::contactUpdated(PsiContact* contact)
{
	ContactListItemProxy* item = findContact(contact);
	if (!item)
		return;
	updateOnlineContactsTimer_->start();
	model_->updatedItem(item);
}

void ContactListGroup::contactGroupsChanged(PsiContact* contact, QStringList contactGroups)
{
	if (contactGroups.isEmpty()) {
		if (contacts_.contains(contact))
			removeContact(contact);
	}
	else if (!findContact(contact)) {
		addContact(contact, contactGroups);
	}

	updateOnlineContactsTimer_->start();
}

ContactListItemProxy* ContactListGroup::item(int index) const
{
	Q_ASSERT(index >= 0);
	Q_ASSERT(index < items_.count());
	return items_.at(index);
}

int ContactListGroup::itemsCount() const
{
	return items_.count();
}

// Some room for optimizations here
int ContactListGroup::indexOf(const ContactListItem* item) const
{
	for (int i = 0; i < items_.count(); ++i)
		if (items_.at(i)->item() == item)
			return i;
	Q_ASSERT(false);
	return -1;
}

ContactListGroup* ContactListGroup::parent() const
{
	return parent_;
}

QModelIndex ContactListGroup::toModelIndex() const
{
	if (!parent())
		return QModelIndex();

	int index = parent()->indexOf(this);
	return model()->itemProxyToModelIndex(parent()->item(index), index);
}

const QVector<ContactListItemProxy*>& ContactListGroup::items() const
{
	return items_;
}

bool ContactListGroup::haveOnlineContacts() const
{
	return haveOnlineContacts_;
}

int ContactListGroup::onlineContactsCount() const
{
	return onlineContactsCount_;
}

int ContactListGroup::totalContactsCount() const
{
	return totalContactsCount_;
}

int ContactListGroup::contactsCount() const
{
	return contacts_.count();
}

void ContactListGroup::updateOnlineContactsFlag()
{
	updateOnlineContactsTimer_->stop();
	if (!parent())
		return;

	bool haveOnlineContacts = false;
	int onlineContactsCount = 0;
	int totalContactsCount = 0;
	foreach(ContactListItemProxy* item, items_) {
		PsiContact* contact     = 0;
		ContactListGroup* group = 0;
		if ((contact = dynamic_cast<PsiContact*>(item->item()))) {
			++totalContactsCount;
			if (contact->isOnline()) {
				haveOnlineContacts = true;
				++onlineContactsCount;
				// break;
			}
		}
		else if ((group = dynamic_cast<ContactListGroup*>(item->item()))) {
			totalContactsCount += group->totalContactsCount();
			if (group->haveOnlineContacts()) {
				haveOnlineContacts = true;
				onlineContactsCount += group->onlineContactsCount();
				// break;
			}
		}
	}

	if (haveOnlineContacts_ != haveOnlineContacts) {
		haveOnlineContacts_ = haveOnlineContacts;
		if (parent()) {
			parent()->updateOnlineContactsFlag();
			model_->updatedGroupVisibility(this);
		}
	}

	if (onlineContactsCount != onlineContactsCount_ ||
	    totalContactsCount != totalContactsCount_)
	{
		onlineContactsCount_ = onlineContactsCount;
		totalContactsCount_ = totalContactsCount;
		if (parent()) {
			model()->updatedItem(parent()->findGroup(this));
		}
	}
}

bool ContactListGroup::isFake() const
{
	if (items_.count() != contacts_.count())
		return false;

	foreach(PsiContact* contact, contacts_) {
		if (!contact->isFake())
			return false;
	}

// FIXME
#ifdef YAPSI
	return name().startsWith(FakeGroupContact::defaultGroupName());
#else
	return false;
#endif
}

bool ContactListGroup::compare(const ContactListItem* other) const
{
	const ContactListGroup* group = dynamic_cast<const ContactListGroup*>(other);
	if (group) {
		if (group->isSpecial()) {
			return !group->compare(this);
		}

		int order = model()->groupState()->groupOrder(group) - model()->groupState()->groupOrder(this);
		if (order) {
			return order > 0;
		}
	}

	return ContactListItem::compare(other);
}

QList<PsiContact*> ContactListGroup::contacts() const
{
	QList<PsiContact*> result;
	contactsHelper(&result);
	return result;
}

void ContactListGroup::contactsHelper(QList<PsiContact*>* contacts) const
{
	foreach(PsiContact* contact, contacts_) {
		if (!contacts->contains(contact))
			contacts->append(contact);
	}

	foreach(ContactListItemProxy* item, items_) {
		ContactListGroup* group = dynamic_cast<ContactListGroup*>(item->item());
		if (group)
			contactsHelper(contacts);
	}
}

#ifdef UNIT_TEST
void ContactListGroup::dumpTree(int indent) const
{
	dumpInfo(this, 0);
	foreach(ContactListItemProxy* item, items_) {
		ContactListGroup* group = dynamic_cast<ContactListGroup*>(item->item());
		if (group)
			dumpTree(indent + 1);
		else
			dumpInfo(item->item(), indent + 1);
	}
}

void ContactListGroup::dumpInfo(const ContactListItem* item, int indent) const
{
	qWarning("%sname = '%s', type = %s", qPrintable(QString(indent, ' ')),
	         qPrintable(item->name()),
	         dynamic_cast<const ContactListGroup*>(item) ? "group" : "contact");
}

void ContactListGroup::dumpTree() const
{
	dumpTree(0);
}
#endif

bool ContactListGroup::isSpecial() const
{
	return false;
}

ContactListGroup::SpecialType ContactListGroup::specialGroupType() const
{
	return SpecialType_None;
}
