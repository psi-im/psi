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

#include "contactlistmodel.h"

#include <QModelIndex>
#include <QVariant>
#include <QColor>
#include <QMessageBox>

#include "psiaccount.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "contactlistitem.h"
#include "contactlistgroup.h"
#include "contactlistnestedgroup.h"
#include "contactlistaccountgroup.h"
#include "contactlistitemproxy.h"
#include "contactlistgroupstate.h"
#include "contactlistgroupcache.h"
#include "contactlistmodelupdater.h"
#include "contactlistspecialgroup.h"
#include "userlist.h"
#include "avatars.h"
#ifdef YAPSI
#include "yacommon.h"
#endif
#include "activity.h"

ContactListModel::ContactListModel(PsiContactList* contactList)
	: QAbstractItemModel(contactList)
	, groupsEnabled_(false)
	, accountsEnabled_(false)
	, contactList_(contactList)
	, updater_(0)
	, rootGroup_(0)
	, bulkUpdateCount_(0)
	, emitDeltaSignals_(true)
	, groupState_(0)
	, groupCache_(0)
{
	groupState_ = new ContactListGroupState(this);
	updater_ = new ContactListModelUpdater(contactList, this);
	groupCache_ = new ContactListGroupCache(this);

	connect(groupState_, SIGNAL(orderChanged()), SLOT(orderChanged()));
	connect(updater_, SIGNAL(addedContact(PsiContact*)), SLOT(addContact(PsiContact*)));
	connect(updater_, SIGNAL(removedContact(PsiContact*)), SLOT(removeContact(PsiContact*)));
	connect(updater_, SIGNAL(contactAlert(PsiContact*)), SLOT(contactAlert(PsiContact*)));
	connect(updater_, SIGNAL(contactAnim(PsiContact*)), SLOT(contactAnim(PsiContact*)));
	connect(updater_, SIGNAL(contactUpdated(PsiContact*)), SLOT(contactUpdated(PsiContact*)));
	connect(updater_, SIGNAL(contactGroupsChanged(PsiContact*)), SLOT(contactGroupsChanged(PsiContact*)));
	connect(updater_, SIGNAL(beginBulkContactUpdate()), SLOT(beginBulkUpdate()));
	connect(updater_, SIGNAL(endBulkContactUpdate()), SLOT(endBulkUpdate()));
	connect(contactList_, SIGNAL(destroying()), SLOT(destroyingContactList()));
	connect(contactList_, SIGNAL(showOfflineChanged(bool)), SIGNAL(showOfflineChanged()));
	connect(contactList_, SIGNAL(showHiddenChanged(bool)), SIGNAL(showHiddenChanged()));
	connect(contactList_, SIGNAL(showSelfChanged(bool)), SIGNAL(showSelfChanged()));
	connect(contactList_, SIGNAL(showAgentsChanged(bool)), SIGNAL(showTransportsChanged()));
	connect(contactList_, SIGNAL(rosterRequestFinished()), SLOT(rosterRequestFinished()));
	connect(contactList_, SIGNAL(contactSortStyleChanged(QString)), SIGNAL(contactSortStyleChanged()));
}

ContactListModel::~ContactListModel()
{
	emitDeltaSignals_ = false;
	delete rootGroup_;
}

void ContactListModel::storeGroupState(const QString& id)
{
	groupState_->load(id);
}

void ContactListModel::updaterCommit()
{
	if (!updater()) {
		Q_ASSERT(false);
		return;
	}
	updater()->commit();
}

void ContactListModel::destroyingContactList()
{
	if (updater_) {
		updater_->commit();
		delete updater_;
		updater_ = 0;
	}

	groupState_->updateGroupList(this);
	groupState_->save();

	contactList_ = 0;
	invalidateLayout();
}

ContactListGroup* ContactListModel::createRootGroup()
{
	if (accountsEnabled_)
		return new ContactListAccountGroup(this, 0, 0);

	if (!groupsEnabled_)
		return new ContactListGroup(this, 0);

	return new ContactListNestedGroup(this, 0, QString());
}

bool ContactListModel::groupsEnabled() const
{
	return groupsEnabled_;
}

void ContactListModel::setGroupsEnabled(bool enabled)
{
	if (groupsEnabled_ != enabled) {
		groupsEnabled_ = enabled;
		invalidateLayout();
	}
}

bool ContactListModel::accountsEnabled() const
{
	return accountsEnabled_;
}

void ContactListModel::setAccountsEnabled(bool enabled)
{
	if (accountsEnabled_ != enabled) {
		accountsEnabled_ = enabled;
		invalidateLayout();
	}
}

void ContactListModel::beginBulkUpdate()
{
	Q_ASSERT(bulkUpdateCount_ >= 0);
	if (bulkUpdateCount_ == 0) {
		emitDeltaSignals_ = false;
		doResetAfterBulkUpdate_ = false;
		doLayoutUpdateAfterBulkUpdate_ = false;

		beginResetModel();

		// blockSignals(true);
		emit layoutAboutToBeChanged();
	}

	++bulkUpdateCount_;
}

void ContactListModel::endBulkUpdate()
{
	--bulkUpdateCount_;
	Q_ASSERT(bulkUpdateCount_ >= 0);

	if (bulkUpdateCount_ == 0) {
		// blockSignals(false);
		emitDeltaSignals_ = true;

		// using begin/endResetModel instead
		/*if (doResetAfterBulkUpdate_) {
			reset();

			// in Qt 4.3.4 emitting modelReset() leads to QSortFilterProxyModel
			// calling invalidate() on itself first, then immediately it will emit
			// modelReset() too.
			//
			// It's very easy to reproduce on yapsi r2193 and Qt 4.3.4: enable groups in roster,
			// then disable them. And all you could see now is blank contacts roster.
			emit layoutAboutToBeChanged();
			emit layoutChanged();
		}
		else if (doLayoutUpdateAfterBulkUpdate_) {
			// emit layoutAboutToBeChanged();
			emit layoutChanged();
		}*/

		endResetModel();
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

void ContactListModel::orderChanged()
{
	emit layoutAboutToBeChanged();
	emit layoutChanged();
}

void ContactListModel::invalidateLayout()
{
	beginBulkUpdate();

	if (rootGroup_)
		delete rootGroup_;

	if (updater()) {
		updater()->clear();
	}

	rootGroup_ = createRootGroup();

	initializeModel();

	endBulkUpdate();
}

void ContactListModel::initializeModel()
{
	if (contactList_ && updater()) {
		foreach(PsiContact* contact, contactList_->contacts()) {
			updater()->addContact(contact);
		}
	}

	foreach(PsiContact* contact, additionalContacts()) {
		addContact(contact);
	}
}

QStringList ContactListModel::filterContactGroups(QStringList groups) const
{
	return groups;
}

void ContactListModel::addContact(PsiContact* contact)
{
	Q_ASSERT(contact);
	if (!accountsEnabled() && contact->isSelf())
		return;

	addContact(contact, contact->groups());
}

void ContactListModel::removeContact(PsiContact* contact)
{
	Q_ASSERT(contact);
	if (!accountsEnabled() && contact->isSelf())
		return;

	contactGroupsChanged(contact, QStringList());
}

void ContactListModel::contactGroupsChanged(PsiContact* contact)
{
	Q_ASSERT(contact);
	if (!accountsEnabled() && contact->isSelf())
		return;

	Q_ASSERT(rootGroup_);
	contactGroupsChanged(contact, contact->groups());
}

void ContactListModel::addContact(PsiContact* contact, QStringList contactGroups)
{
	Q_ASSERT(rootGroup_);
	rootGroup_->addContact(contact, rootGroup_->sanitizeGroupNames(filterContactGroups(contactGroups)));
}

PsiContact* ContactListModel::contactFor(const QModelIndex& index) const
{
	if (indexType(index) != ContactType)
		return 0;

	ContactListItemProxy* proxy = static_cast<ContactListItemProxy*>(index.internalPointer());
	Q_ASSERT(proxy);
	PsiContact* result = dynamic_cast<PsiContact*>(proxy->item());
	Q_ASSERT(result);
	return result;
}

QModelIndexList ContactListModel::indexesFor(PsiContact* contact) const
{
	Q_ASSERT(contact);
	QModelIndexList result;
	foreach(ContactListGroup* group, groupCache()->groupsFor(contact)) {
		result += index(group->indexOf(contact), 0, group->toModelIndex());
	}
	return result;
}

void ContactListModel::contactAlert(PsiContact* contact)
{
	QModelIndexList indexes = indexesFor(contact);
	if (!indexes.isEmpty()) {
		emit contactAlert(indexes.first());
	}
}

void ContactListModel::contactAnim(PsiContact* /*contact*/)
{
}

void ContactListModel::contactUpdated(PsiContact* contact)
{
	Q_ASSERT(rootGroup_);
	// rootGroup_->contactUpdated(contact);
	foreach(ContactListGroup* group, groupCache()->groupsFor(contact)) {
		group->contactUpdated(contact);
	}
}

void ContactListModel::contactGroupsChanged(PsiContact* contact, QStringList contactGroups)
{
	Q_ASSERT(rootGroup_);
	rootGroup_->contactGroupsChanged(contact, rootGroup_->sanitizeGroupNames(filterContactGroups(contactGroups)));
}

void ContactListModel::itemAboutToBeInserted(ContactListGroup* group, int index)
{
	doResetAfterBulkUpdate_ = true;

	if (!emitDeltaSignals_)
		return;

	beginInsertRows(group->toModelIndex(), index, index);
}

void ContactListModel::insertedItem(ContactListGroup* group, int index)
{
	Q_ASSERT(doResetAfterBulkUpdate_);
	Q_UNUSED(group);
	Q_UNUSED(index);

	if (!emitDeltaSignals_)
		return;

	endInsertRows();
}

void ContactListModel::itemAboutToBeRemoved(ContactListGroup* group, int index)
{
	doResetAfterBulkUpdate_ = true;

	if (!emitDeltaSignals_)
		return;

	beginRemoveRows(group->toModelIndex(), index, index);
}

void ContactListModel::removedItem(ContactListGroup* group, int index)
{
	Q_ASSERT(doResetAfterBulkUpdate_);
	Q_UNUSED(group);
	Q_UNUSED(index);

	if (!emitDeltaSignals_)
		return;

	endRemoveRows();
}

void ContactListModel::updatedItem(ContactListItemProxy* item)
{
	doLayoutUpdateAfterBulkUpdate_ = true;

	if (!emitDeltaSignals_)
		return;

	QModelIndex index = itemProxyToModelIndex(item);
	emit dataChanged(index, index);
}

void ContactListModel::updatedGroupVisibility(ContactListGroup* group)
{
	doLayoutUpdateAfterBulkUpdate_ = true;

	if (!emitDeltaSignals_)
		return;

	Q_UNUSED(group);
	// FIXME: Ideally this should work, but in current Qt it doesn't
	// updatedItem(group->parent()->findGroup(group));
	emit layoutAboutToBeChanged();
	emit layoutChanged();
}

QVariant ContactListModel::contactListItemData(const ContactListItem* item, int role) const
{
	Q_ASSERT(item);
	if (role == TypeRole) {
		return QVariant(item->type());
	}
	else if ((role == Qt::DisplayRole || role == Qt::EditRole) /*&& index.column() == NameColumn*/) {
		return QVariant(item->displayName());
	}
	else if (role == ExpandedRole) {
		return QVariant(item->expanded());
	}

	return QVariant();
}

QVariant ContactListModel::contactGroupData(const ContactListGroup* group, int role) const
{
	Q_ASSERT(group);

	if (role == TotalItemsRole) {
		return QVariant(group->itemsCount());
	}
	else if (role == FullGroupNameRole) {
		return QVariant(group->fullName());
	}
	else if (role == OnlineContactsRole) {
		return QVariant(group->onlineContactsCount());
	}
	else if (role == TotalContactsRole) {
		return QVariant(group->totalContactsCount());
	}
	else if (role == InternalGroupNameRole) {
		return QVariant(group->internalGroupName());
	}
	else if (role == SpecialGroupTypeRole) {
		return QVariant(group->specialGroupType());
	}

	return contactListItemData(group, role);
}

QVariant ContactListModel::accountData(const ContactListAccountGroup* account, int role) const
{
	Q_ASSERT(account);
	if (!account->account()) {
		return QVariant();
	}

	if (role == JidRole) {
		return QVariant(account->account()->jid().full());
	}
	// else if (role == PictureRole) {
	// 	return QVariant(account->account()->picture());
	// }
	else if (role == StatusTextRole) {
		return QVariant(account->account()->status().status().simplified());
	}
	else if (role == StatusTypeRole) {
		return QVariant(account->account()->status().type());
	}
	else if (role == OnlineContactsRole) {
		return QVariant(account->account()->onlineContactsCount());
	}
	else if (role == TotalContactsRole) {
		return QVariant(account->account()->contactList().count());
	}
	else if (role == UsingSSLRole) {
		return QVariant(account->account()->usingSSL());
	}
	else if (role == IsAlertingRole) {
		return QVariant(account->account()->alerting());
	}
	else if (role == AlertPictureRole) {
		return QVariant(account->account()->alertPicture());
	}

	return contactGroupData(account, role);
}

QVariant ContactListModel::contactData(const PsiContact* contact, int role) const
{
	Q_ASSERT(contact);

	if (role == JidRole) {
		return QVariant(contact->jid().full());
	}
	else if (role == Qt::ToolTipRole) {
		return QVariant(contact->userListItem().makeTip(true, false));
	}
	else if (role == PictureRole) {
		return QVariant(contact->picture());
	}
	else if (role == StatusTextRole) {
		return QVariant(contact->statusText().simplified());
	}
	else if (role == StatusTypeRole) {
		return QVariant(contact->status().type());
	}
	else if (role == PresenceErrorRole) {
		return QVariant(contact->userListItem().presenceError());
	}
	else if (role == IsAgentRole) {
		return QVariant(contact->isAgent());
	}
	else if (role == AuthorizesToSeeStatusRole) {
		return QVariant(contact->authorizesToSeeStatus());
	}
	else if (role == AskingForAuthRole) {
		return QVariant(contact->askingForAuth());
	}
	else if (role == IsAlertingRole) {
		return QVariant(contact->alerting());
	}
	else if (role == AlertPictureRole) {
		return QVariant(contact->alertPicture());
	}
	else if (role == IsAnimRole) {
		return QVariant(contact->isAnimated());
	}
	else if (role == PhaseRole) {
		return QVariant(false);
	}
	else if (role == MoodRole) {
		const Mood &m = contact->userListItem().mood();
		if(m.isNull())
			return QVariant();
		return QVariant::fromValue(m);
	}
	else if (role == ActivityRole) {
		const Activity &a = contact->userListItem().activity();
		if (!a.isNull())
			return QVariant::fromValue(a);
		return QVariant();
	}
	else if (role == GeolocationRole) {
		return QVariant(!contact->userListItem().geoLocation().isNull());
	}
	else if (role == TuneRole) {
		return QVariant(!contact->userListItem().tune().isEmpty());
	}
	else if (role == ClientRole) {
		return QVariant(contact->userListItem().clients());
	}
	else if (role == AvatarRole) {
		QPixmap pix = contact->isPrivate() ?
			      contact->account()->avatarFactory()->getMucAvatar(contact->jid()) :
			      contact->account()->avatarFactory()->getAvatar(contact->jid());
		return QVariant(pix);
	}
	else if (role == IsMucRole) {
		return QVariant(contact->userListItem().isConference());
	}
	else if (role == MucMessagesRole) {
		return QVariant(contact->userListItem().pending());
	}
	else if (role == BlockRole) {
		return QVariant(contact->isBlocked());
	}
	else if (role == IsSecureRole) {
		return QVariant(contact->userListItem().isSecure());
	}
#ifdef YAPSI
	else if (role == Qt::ForegroundRole) {
		return QVariant(Ya::statusColor(contact->status().type()));
	}
	else if (role == GenderRole) {
		return QVariant(contact->gender());
	}
	else if (role == Qt::DisplayRole || role == Qt::EditRole) {
		return QVariant(Ya::contactName(
							contactListItemData(contact, role).toString(),
							contactData(contact, ContactListModel::JidRole).toString()
						));
	}
#endif

	return contactListItemData(contact, role);
}

QVariant ContactListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	ContactListItemProxy* item = itemProxy(index);
	Q_ASSERT(item);
	Q_ASSERT(item->item());
	return itemData(item->item(), role);
}

QVariant ContactListModel::itemData(const ContactListItem* item, int role) const
{
	Q_ASSERT(item);
	if (const PsiContact* contact = dynamic_cast<const PsiContact*>(item)) {
		return contactData(contact, role);
	}
	else if (const ContactListAccountGroup* accountGroup = dynamic_cast<const ContactListAccountGroup*>(item)) {
		return accountData(accountGroup, role);
	}
	else if (const ContactListGroup* group = dynamic_cast<const ContactListGroup*>(item)) {
		return contactGroupData(group, role);
	}

	return contactListItemData(item, role);
}

bool ContactListModel::setData(const QModelIndex& index, const QVariant& data, int role)
{
	if (!index.isValid())
		return false;

	ContactListItemProxy* item = itemProxy(index);
	if (!item)
		return false;
	ContactListGroup*     group   = 0;
	PsiContact*           contact = 0;

	if (role == ActivateRole) {
		PsiContact* contact = dynamic_cast<PsiContact*>(item->item());
		if (!contact)
			return false;

		contact->activate();
		return true;
	}
	else if (role == Qt::EditRole) {
		QString name = data.toString();
		if ((contact = dynamic_cast<PsiContact*>(item->item()))) {
			//if (name.isEmpty()) {
				// QMessageBox::information(0, tr("Error"), tr("You can't set a blank name."));
			//	return false;
			//}
			//else {
				contact->setName(name);
			//}
		}
		else if ((group = dynamic_cast<ContactListGroup*>(item->item()))) {
			if (name.isEmpty()) {
				QMessageBox::information(0, tr("Error"), tr("You can't set a blank group name."));
				return false;
			}
			// else {
				// // make sure we don't have it already
				// if (group->account()->groupList().contains(name)) {
				// 	QMessageBox::information(0, tr("Error"), tr("You already have a group with that name."));
				// 	return false;
				// }
				group->setName(name);
			// }
		}
		emit dataChanged(index, index);
		return true;
	}
	else if (role == ExpandedRole) {
		if ((group = dynamic_cast<ContactListGroup*>(item->item()))) {
			group->setExpanded(data.toBool());
		}
	}

	return true;
}

QVariant ContactListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(section);
	Q_UNUSED(orientation);
	Q_UNUSED(role);
	return QVariant();
}

/**
 * Returns the model item index for the item in the \param parent
 * with the given \param row and \param column.
 */
QModelIndex ContactListModel::index(int row, int column, const QModelIndex &parent) const
{
	if (column < 0 || column >= 1 || row < 0 || parent.column() > 0)
		return QModelIndex();

	ContactListGroup* group = 0;
	if (!parent.isValid()) {
		group = rootGroup_;
	}
	else {
		ContactListItemProxy* item = itemProxy(parent);
		group = dynamic_cast<ContactListGroup*>(item->item());
	}

	if (group && row < group->itemsCount())
		return itemProxyToModelIndex(group->item(row), row);

	return QModelIndex();
}

/**
 * Return the parent of the given \param index model item.
 */
QModelIndex ContactListModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	ContactListItemProxy* item = itemProxy(index);
	if (item && item->parent())
		return item->parent()->toModelIndex();

	return QModelIndex();
}

/**
 * Returns the number of rows in the \param parent model index.
 */
int ContactListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	ContactListGroup* group = 0;
	if (parent.isValid()) {
		ContactListItemProxy* item = itemProxy(parent);
		group = dynamic_cast<ContactListGroup*>(item->item());
	}
	else {
		group = rootGroup_;
	}

	return group ? group->itemsCount() : 0;
}

/**
 * Returns the number of columns in the \param parent model item.
 */
int ContactListModel::columnCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;
	return 1;
}

/**
 * Returns the item flags for the given \param index in the model.
 */
Qt::ItemFlags ContactListModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::ItemIsDropEnabled;

	Qt::ItemFlags f = QAbstractItemModel::flags(index);
	f |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

	ContactListItemProxy* item = itemProxy(index);
	if ((index.column() == NameColumn) && item && item->item()->isEditable())
		f |= Qt::ItemIsEditable;
	return f;
}

/**
 * Returns true if the \param parent model item has children; otherwise
 * returns false.
 */
bool ContactListModel::hasChildren(const QModelIndex& parent)
{
	return rowCount(parent) > 0;
}

/**
 * Call this slot to notify \param index that it's now displayed in the 'expanded' state.
 */
void ContactListModel::expanded(const QModelIndex& index)
{
	setData(index, QVariant(true), ExpandedRole);
}

/**
* Call this slot to notify \param index that it's now displayed in the 'collapsed' state.
 */
void ContactListModel::collapsed(const QModelIndex& index)
{
	setData(index, QVariant(false), ExpandedRole);
}

PsiContactList* ContactListModel::contactList() const
{
	return contactList_;
}

ContactListItemProxy* ContactListModel::modelIndexToItemProxy(const QModelIndex& index) const
{
	return itemProxy(index);
}

QModelIndex ContactListModel::itemProxyToModelIndex(ContactListItemProxy* item) const
{
	Q_ASSERT(item);
	int row = item->parent() ? item->parent()->indexOf(item->item()) : 0;
	return itemProxyToModelIndex(item, row);
}

QModelIndex ContactListModel::itemProxyToModelIndex(ContactListItemProxy* item, int index) const
{
	Q_ASSERT(item);
	return createIndex(index, 0, item);
}

void ContactListModel::renameSelectedItem()
{
	emit inPlaceRename();
}

ContactListModel::Type ContactListModel::indexType(const QModelIndex& index)
{
	QVariant type = index.data(ContactListModel::TypeRole);
	if (type.isValid())
		return static_cast<ContactListModel::Type>(type.toInt());

	return ContactListModel::InvalidType;
}

bool ContactListModel::isGroupType(const QModelIndex& index)
{
	return indexType(index) == ContactListModel::GroupType ||
		   indexType(index) == ContactListModel::AccountType;
}

ContactListItemProxy* ContactListModel::itemProxy(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0) || (index.model() != this))
		return 0;
	ContactListItemProxy* proxy = static_cast<ContactListItemProxy*>(index.internalPointer());
#if 0
	if (contactListItemProxyHash_.contains(proxy) && !contactListItemProxyHash_[proxy].isNull())
		return proxy;
	return 0;
#else
	return proxy;
#endif
}

PsiAccount* ContactListModel::account(const QModelIndex& index) const
{
	ContactListItemProxy* item = itemProxy(index);
	if (item) {
		PsiContact* contact = dynamic_cast<PsiContact*>(item->item());
		if (contact)
			return contact->account();
	}
	return 0;
}

ContactListGroupState* ContactListModel::groupState() const
{
	return groupState_;
}

ContactListModelUpdater* ContactListModel::updater() const
{
	return updater_;
}

ContactListGroupCache* ContactListModel::groupCache() const
{
	return groupCache_;
}

ContactListGroup* ContactListModel::rootGroup() const
{
	return rootGroup_;
}

QList<PsiContact*> ContactListModel::additionalContacts() const
{
	return QList<PsiContact*>();
}

int ContactListModel::groupOrder(const QString& groupFullName) const
{
	ContactListGroup* group = groupCache()->findGroup(groupFullName);
	if (group) {
		return groupState()->groupOrder(group);
	}
	return 0;
}

void ContactListModel::setGroupOrder(const QString& groupFullName, int order)
{
	ContactListGroup* group = groupCache()->findGroup(groupFullName);
	if (group) {
		groupState()->setGroupOrder(group, order);
	}
}

bool ContactListModel::showOffline() const
{
	if (!contactList_)
		return false;
	return contactList_->showOffline();
}

bool ContactListModel::showSelf() const
{
	if (!contactList_)
		return false;
	return contactList_->showSelf();
}

bool ContactListModel::showTransports() const
{
	if (!contactList_)
		return false;
	return contactList_->showAgents();
}

bool ContactListModel::showHidden() const
{
	if (!contactList_)
		return false;
	return contactList_->showHidden();
}

QString ContactListModel::contactSortStyle() const
{
	if (!contactList_)
		return QString("alpha");
	return contactList_->contactSortStyle();
}

bool ContactListModel::updatesEnabled() const
{
	return updater()->updatesEnabled();
}

void ContactListModel::setUpdatesEnabled(bool updatesEnabled)
{
	updater()->setUpdatesEnabled(updatesEnabled);
}

bool ContactListModel::hasContacts(bool onlineOnly) const
{
	return groupCache()->hasContacts(onlineOnly);
}

QModelIndex ContactListModel::groupToIndex(ContactListGroup* group) const
{
	if (group && group->parent()) {
		int groupIndex = group->parent()->indexOf(group);
		if (groupIndex >= 0) {
			ContactListItemProxy* itemProxy = group->parent()->item(groupIndex);
			if (itemProxy) {
				QModelIndex index = itemProxyToModelIndex(itemProxy);
				return index;
			}
		}
	}

	return QModelIndex();
}

void ContactListModel::contactListItemProxyCreated(ContactListItemProxy* proxy)
{
	Q_UNUSED(proxy);
#if 0
	contactListItemProxyHash_[proxy] = proxy;
#endif
}
