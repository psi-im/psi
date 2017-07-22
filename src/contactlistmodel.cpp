/*
 * contactlistmodel.cpp - model of contact list
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactlistmodel_p.h"

#include "debug.h"
#include "psiaccount.h"
#include "psicontactlist.h"
#include "contactlistitem.h"
#include "userlist.h"
#include "avatars.h"
#include "psioptions.h"

#include <QMessageBox>
#include <QModelIndex>
#include <QVariant>
#include <QColor>
#include <QIcon>
#include <QTextDocument>

#define MAX_COMMIT_DELAY 30 /* seconds */
#define COMMIT_INTERVAL 100 /* msecs */

#define COLLAPSED_OPTIONS "options.main-window.contact-list.group-state.collapsed"
#define HIDDEN_OPTIONS "options.main-window.contact-list.group-state.hidden"

/*****************************/
/* ContactListModel::Private */
/*****************************/

ContactListModel::Private::Private(ContactListModel *parent)
	: QObject()
	, q(parent)
	, groupsEnabled(false)
	, accountsEnabled(false)
	, contactList(nullptr)
	, commitTimer(new QTimer(this))
	, commitTimerStartTime()
	, monitoredContacts()
	, operationQueue()
	, collapsed()
	, hidden()
{
	connect(commitTimer, SIGNAL(timeout()), SLOT(commit()));
	commitTimer->setSingleShot(true);
	commitTimer->setInterval(COMMIT_INTERVAL);

	collapsed = PsiOptions::instance()->getOption(COLLAPSED_OPTIONS).toStringList();
	hidden = PsiOptions::instance()->getOption(HIDDEN_OPTIONS).toStringList();
}


ContactListModel::Private::~Private()
{
}

void ContactListModel::Private::realAddContact(PsiContact *contact)
{
	ContactListItem *root = static_cast<ContactListItem*>(q->root());;
	if (accountsEnabled) {
		PsiAccount *account = contact->account();
		ContactListItem *accountItem = root->findAccount(account);

		if (!accountItem) {
			accountItem = new ContactListItem(q, ContactListItem::Type::AccountType);
			accountItem->setAccount(account);
			accountItem->setExpanded(!collapsed.contains(accountItem->internalName()));

			connect(account, SIGNAL(accountDestroyed()), SLOT(onAccountDestroyed()));
			connect(account, SIGNAL(updatedAccount()), SLOT(updateAccount()));

			root->appendChild(accountItem);
		}
		root = accountItem;
	}

	if (!contact->isSelf() && groupsEnabled) {
		ContactListItem::SpecialGroupType specialGroupType = specialGroupFor(contact);
		QStringList groups = specialGroupType == ContactListItem::SpecialGroupType::NoneSpecialGroupType ? contact->groups() : QStringList{QString()};

		for (const QString &groupName: groups) {

			ContactListItem *groupItem = nullptr;
			if (specialGroupType == ContactListItem::SpecialGroupType::NoneSpecialGroupType)
				groupItem = root->findGroup(groupName);
			else
				groupItem = root->findGroup(specialGroupType);

			// No duplicates
			if (groupItem && groupItem->findContact(contact)) {
				continue;
			}

			ContactListItem *item = new ContactListItem(q, ContactListItem::Type::ContactType);
			item->setContact(contact);

			if (!groupItem) {
				groupItem = new ContactListItem(q, ContactListItem::Type::GroupType, specialGroupType);
				if (specialGroupType == ContactListItem::SpecialGroupType::NoneSpecialGroupType)
					groupItem->setName(groupName);

				root->appendChild(groupItem);
				groupItem->setExpanded(!collapsed.contains(groupItem->internalName()));
				groupItem->setHidden(hidden.contains(groupItem->internalName()));
			}
			groupItem->appendChild(item);

			monitoredContacts.insertMulti(contact, q->toModelIndex(item));
		}
	}
	else {
		ContactListItem *item = new ContactListItem(q, ContactListItem::Type::ContactType);
		item->setContact(contact);
		root->appendChild(item);
		monitoredContacts.insertMulti(contact, q->toModelIndex(item));
	}

	connect(contact, SIGNAL(destroyed(PsiContact*)), SLOT(removeContact(PsiContact*)));
	connect(contact, SIGNAL(groupsChanged()), SLOT(contactGroupsChanged()));
	connect(contact, SIGNAL(updated()), SLOT(contactUpdated()));
	connect(contact, SIGNAL(alert()), SLOT(contactUpdated()));
	connect(contact, SIGNAL(anim()), SLOT(contactUpdated()));
}

void ContactListModel::Private::addContacts(const QList<PsiContact*> &contacts)
{
	SLOW_TIMER(100);

	if (contacts.isEmpty())
		return;

	emit q->layoutAboutToBeChanged();
	for (auto *contact: contacts) {
		realAddContact(contact);
	}
	emit q->layoutChanged();
}

void ContactListModel::Private::updateContacts(const QList<PsiContact*> &contacts)
{
	SLOW_TIMER(100);

	if (contacts.isEmpty())
		return;

	// prepare ranges for updating to reduce 'emit dataChanged' invoking
	QHash<QModelIndex, QPair<int, int>> ranges;
	QModelIndexList indexes;
	for (const PsiContact *contact: contacts) {
		QModelIndexList indexes2 = q->indexesFor(contact);
		indexes += indexes2;

		for (const QModelIndex &index: indexes2) {
			QModelIndex parent = index.parent();
			int row = index.row();
			if (ranges.contains(parent)) {
				if (index.row() < ranges.value(parent).first)
					ranges[parent].first = row;
				else if (index.row() > ranges.value(parent).second)
					ranges[parent].second = row;
			}
			else {
				ranges.insert(parent, QPair<int, int>(row, row));
			}
		}
	}


	QHashIterator<QModelIndex, QPair<int, int>> it(ranges);
	while (it.hasNext()) {
		it.next();
		int row1 = it.value().first;
		int row2 = it.value().second;
		QModelIndex index = it.key();

		// update contact
		emit q->dataChanged(index.child(row1, 0), index.child(row2, 0));

		// Update group
		emit q->dataChanged(index, index);
	}
}

void ContactListModel::Private::addOperation(PsiContact *contact, ContactListModel::Private::Operation operation)
{
	if (operation == AddContact && monitoredContacts.contains(contact)) {
		Q_ASSERT(false);
	}

	if (!operationQueue.contains(contact)) {
		operationQueue[contact] = operation;
	}
	else {
		operationQueue[contact] |= operation;
	}

	if (commitTimerStartTime.isNull())
		commitTimerStartTime = QDateTime::currentDateTime();


	if (commitTimerStartTime.secsTo(QDateTime::currentDateTime()) > MAX_COMMIT_DELAY)
		commit();
	else
		commitTimer->start();
}

int ContactListModel::Private::simplifiedOperationList(int operations) const
{
	return (operations & AddContact)
			? AddContact
			: operations;
}

// Detect special group type for contact
ContactListItem::SpecialGroupType ContactListModel::Private::specialGroupFor(PsiContact *contact)
{
	ContactListItem::SpecialGroupType type = ContactListItem::SpecialGroupType::NoneSpecialGroupType;

	if (contact->isPrivate()) {
		type = ContactListItem::SpecialGroupType::MucPrivateChatsSpecialGroupType;
	}
	else if (!contact->inList()) {
		type = ContactListItem::SpecialGroupType::NotInListSpecialGroupType;
	}
	else if (contact->isAgent()) {
		type = ContactListItem::SpecialGroupType::TransportsSpecialGroupType;
	}
	else if (contact->isConference()) {
		type = ContactListItem::SpecialGroupType::ConferenceSpecialGroupType;
	}
	else if (contact->noGroups()) {
		type = ContactListItem::SpecialGroupType::GeneralSpecialGroupType;
	}

	return type;
}

void ContactListModel::Private::commit()
{
	SLOW_TIMER(100);

	commitTimerStartTime = QDateTime();

	commitTimer->stop();
	if (operationQueue.isEmpty())
		return;

	QHashIterator<PsiContact*, int> it(operationQueue);

	QList<PsiContact*> contactsForAdding;
	QList<PsiContact*> contactsForUpdate;


	while (it.hasNext()) {
		it.next();
		PsiContact *contact = it.key();

		int operations = simplifiedOperationList(it.value());
		if (operations & AddContact) {
			Q_ASSERT(!monitoredContacts.contains(contact));
			if (monitoredContacts.contains(contact))
				continue;

			contactsForAdding << contact;

		}
		if (operations & RemoveContact)
			Q_ASSERT(false);
		if (operations & UpdateContact)
			contactsForUpdate << contact;
		if (operations & ContactGroupsChanged) {
			removeContact(contact);
			if (!contactsForAdding.contains(contact)) {
				contactsForAdding << contact;
			}
		}
	}

	addContacts(contactsForAdding);
	updateContacts(contactsForUpdate);
	operationQueue.clear();
}

void ContactListModel::Private::clear()
{
	q->beginResetModel();

	qDeleteAll(q->root()->children());

	operationQueue.clear();

	QHashIterator<PsiContact*, QPersistentModelIndex> it(monitoredContacts);
	while (it.hasNext()) {
		it.next();
		disconnect(it.key(), 0, this, 0);
	}
	monitoredContacts.clear();

	q->endResetModel();
}

void ContactListModel::Private::addContact(PsiContact *contact)
{
	Q_ASSERT(!monitoredContacts.contains(contact));
	if (monitoredContacts.contains(contact))
		return;

	addOperation(contact, AddContact);
}

QModelIndex ContactListModel::Private::findItemRecursvive(PsiContact *contact, QModelIndex index)
{
	for (int i = 0; i < q->rowCount(index); i++) {
		QModelIndex childIndex = q->index(i, 0, index);
		auto clitem = static_cast<ContactListItem*>(childIndex.internalPointer());

		if (clitem->isContact() && clitem->contact() == contact) {
			return childIndex;
		}

		if (clitem->children().count()) {
			childIndex = findItemRecursvive(contact, childIndex);
			if (childIndex.isValid()) {
				return childIndex;
			}
		}
	}
	return QModelIndex();
}

/*!
 * removeContact() could be called directly by PsiContactList when account is disabled, or
 * by contact's destructor. We just ensure that the calls are balanced.
 */

void ContactListModel::Private::removeContact(PsiContact *contact)
{
	Q_ASSERT(monitoredContacts.contains(contact));
	if (!monitoredContacts.contains(contact))
		return;

	while (monitoredContacts.contains(contact)) {
		QModelIndex index = monitoredContacts.take(contact);

		if (!index.isValid()) {
			// crap! how?? gotta check them all.. ugly workaround for a crash
			index = findItemRecursvive(contact);
		}
		if (!index.isValid()) {
			continue; // hmm.. that's strange
		}

		q->beginRemoveRows(index.parent(), index.row(), index.row());
		ContactListItem *item = q->toItem(index);
		ContactListItem *group = item->parent();

		delete item;
		q->endRemoveRows();

		index = q->toModelIndex(group);
		// Delete empty group
		if (group->isGroup() && !group->childCount()) {
			q->beginRemoveRows(index.parent(), index.row(), index.row());
			delete group;
			q->endRemoveRows();

		}
		else {
			// Update group
			emit q->dataChanged(index, index);
		}
	}
	disconnect(contact, 0, this, 0);
	operationQueue.remove(contact);
}

void ContactListModel::Private::contactUpdated()
{
	PsiContact *contact = qobject_cast<PsiContact*>(sender());
	Q_ASSERT(monitoredContacts.contains(contact));
	if (!monitoredContacts.contains(contact))
		return;

	// Check for groups changing
	// Maybe very difficult and should be simplified?
	QList<ContactListItem*> groupItems;
	for (const QPersistentModelIndex &index: monitoredContacts.values(contact)) {
		ContactListItem *item = q->toItem(index);
		if (item->parent() && item->parent()->isGroup()) {
			groupItems << item;
		}
	}

	Operation operation = Operation::UpdateContact;
	ContactListItem::SpecialGroupType specialGroupType = specialGroupFor(contact);
	if (specialGroupType == ContactListItem::SpecialGroupType::NoneSpecialGroupType) {
		QStringList groups1;
		for (ContactListItem *item: groupItems) {
			groups1 << item->name();
		}
		groups1.sort();
		QStringList groups2 = contact->groups();
		groups2.sort();
		if (groups1 != groups2) {
			operation = Operation::ContactGroupsChanged;
		}
	}
	else if (groupItems.size() > 1 || (!groupItems.isEmpty() && groupItems.first()->specialGroupType() != specialGroupType)) {
		operation = Operation::ContactGroupsChanged;
	}

	addOperation(contact, operation);
}

void ContactListModel::Private::contactGroupsChanged()
{
	PsiContact* contact = qobject_cast<PsiContact*>(sender());
	Q_ASSERT(monitoredContacts.contains(contact));
	if (!monitoredContacts.contains(contact))
		return;

	addOperation(contact, ContactGroupsChanged);
}

void ContactListModel::Private::onAccountDestroyed()
{
	PsiAccount *account = qobject_cast<PsiAccount*>(sender());
	ContactListItem *root = static_cast<ContactListItem*>(q->root());
	ContactListItem *item = root->findAccount(account);
	if (!item) {
		qCritical("BUG: account was already removed from the list");
		return;
	}
	QModelIndex index = q->toModelIndex(item);
	q->beginRemoveRows(index.parent(), index.row(), index.row());
	delete item;
	q->endRemoveRows();
}

void ContactListModel::Private::updateAccount()
{
	PsiAccount *account = qobject_cast<PsiAccount*>(sender());
	ContactListItem *root = static_cast<ContactListItem*>(q->root());;
	ContactListItem *accountItem = root->findAccount(account);
	Q_ASSERT(accountItem);
	q->updateItem(accountItem);
}

/********************/
/* ContactListModel */
/********************/

ContactListModel::ContactListModel(PsiContactList* contactList)
	: AbstractTreeModel(new ContactListItem(this, ContactListItem::Type::RootType))
	, d(new Private(this))
{
	d->contactList = contactList;

	connect(contactList, SIGNAL(addedContact(PsiContact*)), d, SLOT(addContact(PsiContact*)));
	connect(contactList, SIGNAL(removedContact(PsiContact*)), d, SLOT(removeContact(PsiContact*)));

	connect(d->contactList, SIGNAL(destroying()), SLOT(destroyingContactList()));
	connect(d->contactList, SIGNAL(showOfflineChanged(bool)), SIGNAL(showOfflineChanged()));
	connect(d->contactList, SIGNAL(showHiddenChanged(bool)), SIGNAL(showHiddenChanged()));
	connect(d->contactList, SIGNAL(showSelfChanged(bool)), SIGNAL(showSelfChanged()));
	connect(d->contactList, SIGNAL(showAgentsChanged(bool)), SIGNAL(showTransportsChanged()));
	connect(d->contactList, SIGNAL(rosterRequestFinished()), SLOT(rosterRequestFinished()));
	connect(d->contactList, SIGNAL(contactSortStyleChanged(QString)), SIGNAL(contactSortStyleChanged()));
}

ContactListModel::~ContactListModel()
{
	delete d;
}

void ContactListModel::destroyingContactList()
{
	d->commit();

	// Save groups state
	d->collapsed.clear();
	QList<ContactListItem*> list = static_cast<ContactListItem*>(root())->allChildren();

	for (const auto *item: list) {
		if (!item->expanded())
			d->collapsed << item->internalName();

		if (item->isHidden())
			d->hidden << item->internalName();
	}

	PsiOptions::instance()->setOption(COLLAPSED_OPTIONS, d->collapsed);
	PsiOptions::instance()->setOption(HIDDEN_OPTIONS, d->hidden);


	d->clear();

	d->contactList = nullptr;
	invalidateLayout();
}

bool ContactListModel::groupsEnabled() const
{
	return d->groupsEnabled;
}

void ContactListModel::setGroupsEnabled(bool enabled)
{
	if (d->groupsEnabled != enabled) {
		d->groupsEnabled = enabled;
		invalidateLayout();
	}
}

bool ContactListModel::accountsEnabled() const
{
	return d->accountsEnabled;
}

void ContactListModel::setAccountsEnabled(bool enabled)
{
	if (d->accountsEnabled != enabled) {
		d->accountsEnabled = enabled;
		invalidateLayout();
	}
}

void ContactListModel::rosterRequestFinished()
{
	if (rowCount(QModelIndex()) == 0) {
		beginResetModel();
		endResetModel();

		emit layoutAboutToBeChanged();
		emit layoutChanged();
	}
}

void ContactListModel::invalidateLayout()
{
	d->clear();

	if (!d->contactList)
		return;

	beginResetModel();

	for (PsiContact *contact: d->contactList->contacts()) {
		Q_ASSERT(!d->monitoredContacts.contains(contact));
		if (d->monitoredContacts.contains(contact))
			continue;

		d->realAddContact(contact);
	}
	endResetModel();
}

PsiContact* ContactListModel::contactFor(const QModelIndex& index) const
{
	ContactListItem *item = static_cast<ContactListItem*>(index.internalPointer());
	if (item->type() != ContactListItem::Type::ContactType)
		return nullptr;

	return item->contact();
}

QModelIndexList ContactListModel::indexesFor(const PsiContact *contact) const
{
	Q_ASSERT(contact);
	QModelIndexList result;
	for (const auto &index: d->monitoredContacts.values(const_cast<PsiContact*>(contact))) {
		result << index;
	}
	return result;
}

ContactListItem *ContactListModel::toItem(const QModelIndex &index) const
{
	Q_ASSERT(!index.isValid() || index.model() == this);

	ContactListItem *item = nullptr;
	bool b = index.isValid();
	if (!b) {
		item = static_cast<ContactListItem*>(root());
	}
	else {
		item = static_cast<ContactListItem*>(index.internalPointer());
	}

	return item;
}

QModelIndex ContactListModel::toModelIndex(ContactListItem *item) const
{
	if (!item || item == root())
		return QModelIndex();
	else
		return createIndex(item->row(), 0, item);
}

QVariant ContactListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	ContactListItem* item = toItem(index);

	if (role == ContactListItemRole)
		return QVariant::fromValue(item);
	else
		return item->value(role);
}

bool ContactListModel::setData(const QModelIndex &index, const QVariant &data, int role)
{
	if (!index.isValid())
		return false;

	ContactListItem *item = toItem(index);
	if (!item)
		return false;
	PsiContact *contact = item->isContact() ? item->contact() : nullptr;

	if (role == ActivateRole) {
		if (!contact)
			return false;

		contact->activate();
		return true;
	}
	else if (role == Qt::EditRole) {
		QString name = data.toString();
		if (contact) {
			item->setName(name);
			emit dataChanged(index, index);
		}
		else if (item->isGroup() && !name.isEmpty()) {
			QString oldName = item->name();
			QList<PsiContact*> contacts;
			for (int i = 0; i < item->childCount(); ++i) {
				if (item->child(i)->isContact())
				contacts << item->child(i)->contact();
			}

			for (PsiContact *contact: contacts) {
				QStringList groups = contact->groups();
				groups.removeOne(oldName);
				groups << name;
				contact->setGroups(groups);
			}

		}

		return true;
	}
	else if (role == ExpandedRole) {
		if (!item->isContact()) {
			item->setExpanded(data.toBool());
		}
	}

	return true;
}

/**
 * Returns the number of columns in the \param parent model item.
 */
int ContactListModel::columnCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	return 1;
}

/**
 * Returns the item flags for the given \param index in the model.
 */
Qt::ItemFlags ContactListModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::ItemIsDropEnabled;

	Qt::ItemFlags f = QAbstractItemModel::flags(index);
	f |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

	ContactListItem *item = toItem(index);
	if ((index.column() == NameColumn) && item && item->isEditable())
		f |= Qt::ItemIsEditable;

#ifdef HAVE_QT5
	if (!item->isExpandable())
		f |= Qt::ItemNeverHasChildren;
#endif

	return f;
}

/**
 * Call this slot to notify \param index that it's now displayed in the 'expanded' state.
 */
void ContactListModel::expanded(const QModelIndex &index)
{
	setData(index, QVariant(true), ExpandedRole);
}

/**
* Call this slot to notify \param index that it's now displayed in the 'collapsed' state.
 */
void ContactListModel::collapsed(const QModelIndex &index)
{
	setData(index, QVariant(false), ExpandedRole);
}

PsiContactList* ContactListModel::contactList() const
{
	return d->contactList;
}

void ContactListModel::renameSelectedItem()
{
	emit inPlaceRename();
}

void ContactListModel::updateItem(ContactListItem *item)
{
	Q_ASSERT(item);

	QModelIndex index = toModelIndex(item);
	emit dataChanged(index, index);
}

bool ContactListModel::showOffline() const
{
	if (!d->contactList)
		return false;
	return d->contactList->showOffline();
}

bool ContactListModel::showSelf() const
{
	if (!d->contactList)
		return false;
	return d->contactList->showSelf();
}

bool ContactListModel::showTransports() const
{
	if (!d->contactList)
		return false;
	return d->contactList->showAgents();
}

bool ContactListModel::showHidden() const
{
	if (!d->contactList)
		return false;
	return d->contactList->showHidden();
}

QString ContactListModel::contactSortStyle() const
{
	if (!d->contactList)
		return QString("alpha");
	return d->contactList->contactSortStyle();
}
