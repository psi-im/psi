/*
 * contactlistdragmodel.cpp - ContactListModel with support for Drag'n'Drop operations
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

#include "contactlistdragmodel.h"

#include <QtAlgorithms>
#include <QTimer>

#include "psioptions.h"
#include "psiaccount.h"
#include "psicontactlist.h"
#include "psicontact.h"
#include "common.h"
#include "contactlistmodelselection.h"
#include "contactlistitem.h"
#include "debug.h"

//----------------------------------------------------------------------------
// ContactListModelOperationList
//----------------------------------------------------------------------------

ContactListModelOperationList::ContactListModelOperationList(Action action)
	: action_(action)
{
}

ContactListModelOperationList::ContactListModelOperationList(Qt::DropAction action)
{
	if (action == Qt::CopyAction)
		action_ = Copy;
	else if (action == Qt::MoveAction)
		action_ = Move;
	else {
		Q_ASSERT_X(false, "ContactListModelOperationList", "Passed incorrect Qt::DropAction");
	}
}

ContactListModelOperationList::Action ContactListModelOperationList::action() const
{
	return action_;
}

void ContactListModelOperationList::addOperation(PsiContact* contact, const QString& groupFrom, const QString& groupTo)
{
	if (!contact) {
		qWarning("ContactListModelOperationList::addOperation(): contact is NULL");
		return;
	}

	if (!contact->isEditable()) {
		bool deleteOperation = contact->isRemovable() && groupTo.isEmpty();
		if (!deleteOperation) {
			qWarning("ContactListModelOperationList::addOperation(): contact is not editable '%s'", qPrintable(contact->jid().full()));
			return;
		}
	}

	if (!contact->groupOperationPermitted(groupFrom, groupTo)) {
		qWarning("ContactListModelOperationList::addOperation(): contact '%s' refused group operation ('%s' -> '%s')", qPrintable(contact->jid().full()), qPrintable(groupFrom), qPrintable(groupTo));
		return;
	}

	Operation op(groupFrom, groupTo);

	if (!operations_.contains(contact))
		operations_[contact] = QList<Operation>();

	operations_[contact] += op;
}

QList<ContactListModelOperationList::ContactOperation> ContactListModelOperationList::operations() const
{
	QList<ContactOperation> result;

	QHash<PsiContact*, QList<Operation> >::const_iterator it;
	for (it = operations_.constBegin(); it != operations_.constEnd(); ++it) {
		ContactOperation op;
		op.contact    = it.key();
		op.operations = it.value();
		result += op;
	}

	return result;
}

void ContactListModelOperationList::removeAccidentalContactMoveOperations()
{
	if (action_ != Move)
		return;

	QList<PsiContact*> contacts = operations_.keys();
	foreach(PsiContact* psiContact, contacts) {
		bool remove = false;
		foreach(Operation op, operations_[psiContact]) {
			if (psiContact->groups().contains(op.groupTo)) {
				remove = true;
				break;
			}
		}

		if (remove)
			operations_.remove(psiContact);
	}
}

//----------------------------------------------------------------------------
// ContactListDragModel
//----------------------------------------------------------------------------

ContactListDragModel::ContactListDragModel(PsiContactList *contactList)
	: ContactListModel(contactList)
{
}

Qt::DropActions ContactListDragModel::supportedDragActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions ContactListDragModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

Qt::ItemFlags ContactListDragModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = ContactListModel::flags(index);

	ContactListItem *item = static_cast<ContactListItem*>(index.internalPointer());

	if (item) {
		return f | Qt::ItemIsDropEnabled | (item->isDragEnabled() ? Qt::ItemIsDragEnabled : f);
	}

	if (!index.isValid()) {
		return f | Qt::ItemIsDropEnabled;
	}

	return f;
}

QStringList ContactListDragModel::mimeTypes() const
{
	return QStringList()
		   << ContactListModelSelection::mimeType()
		   << "text/plain";
}

QMimeData* ContactListDragModel::mimeData(const QModelIndexList& indexes) const
{
	QList<ContactListItem*> items;

	for (const auto &index: indexes) {
		if (!index.isValid())
			continue;

		ContactListItem *item = toItem(index);
		if (!item)
			continue;

		items << item;
	}

	return new ContactListModelSelection(items);
}

QModelIndexList ContactListDragModel::indexesFor(const QMimeData* data) const
{
	QModelIndexList result;
	ContactListModelSelection selection(data);
	if (!selection.haveRosterSelection() || !contactList())
		return result;

	for (ContactListModelSelection::Contact contact: selection.contacts()) {
		PsiAccount* account = contactList()->getAccount(contact.account);
		if (!account)
			continue;
		PsiContact* psiContact = account->findContact(contact.jid);
		if (!psiContact) {
			if (account->selfContact()->jid() == contact.jid)
				psiContact = account->selfContact();
		}

		if (!psiContact)
			continue;

		QModelIndexList indexes = ContactListModel::indexesFor(psiContact);
		result += indexes;
	}

	for (ContactListModelSelection::Group group: selection.groups()) {
		ContactListItem *item = static_cast<ContactListItem*>(root())->findGroup(group.fullName);
		QModelIndex index = this->toModelIndex(item);
		if (!result.contains(index))
			result << index;
	}

//	for (ContactListModelSelection::Account account: selection.accounts()) {
//		PsiAccount* acc = contactList()->getAccount(account.id);
//		if (!acc)
//			continue;
//		ContactListAccountGroup* rootGroup = dynamic_cast<ContactListAccountGroup*>(this->rootGroup());
//		if (!rootGroup)
//			continue;
//		ContactListGroup* accountGroup = rootGroup->findAccount(acc);
//		QModelIndex index = groupToIndex(accountGroup);
//		if (!result.contains(index))
//			result << index;
//	}

	return result;
}

bool ContactListDragModel::supportsMimeDataOnIndex(const QMimeData* data, const QModelIndex& parent) const
{
	if ((!groupsEnabled() && !accountsEnabled()) || !ContactListModelSelection(data).haveRosterSelection()) {
		return false;
	}

	// disable dragging to special groups
	ContactListItem *item = toItem(parent);
	ContactListItem *group = nullptr;

	if (item->isGroup())
		group = item;
	else if (item->parent() && item->parent()->isGroup())
		group = item->parent();

	if (group && !group->isEditable())
		return false;

	for (const QModelIndex &index: indexesFor(data)) {
		if (index == parent) {
			return false;
		}

		// protection against dragging parent group inside its child
		ContactListItem *item = toItem(index);
		if (item->isContact() && accountsEnabled()) {

			// disable dragging to self contacts
			ContactListItem *parentItem = toItem(parent);
			if (parentItem->isContact() && parentItem->contact()->isSelf())
				return false;

			// disable dragging to different accounts and to self account
			ContactListItem *accountItem = parentItem;
			while (!accountItem->isRoot() && !accountItem->isAccount()) {
				accountItem = accountItem->parent();
			}

			if (accountItem->isRoot())
				return false;

			return accountItem != parentItem && accountItem->account() == item->account();
		}
	}

	return true;
}

bool ContactListDragModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	Q_UNUSED(row);
	Q_UNUSED(column);

	ContactListModelSelection selection(data);
	if (!selection.haveRosterSelection() || !contactList())
		return false;

	for (const ContactListModelSelection::Contact &contact: selection.contacts()) {
		PsiAccount *account = contactList()->getAccount(contact.account);
		PsiContact *psiContact = account ? account->findContact(contact.jid) : nullptr;

		if (psiContact) {
			QStringList groups;
			if (action == Qt::DropAction::CopyAction && !psiContact->noGroups()) {
				groups = psiContact->groups();
			}
			groups << getDropGroupName(parent);
			psiContact->setGroups(groups);
		}
	}


	return true;
}

void ContactListDragModel::renameGroup(ContactListItem *group, const QString &newName)
{
	Q_UNUSED(group);
	Q_UNUSED(newName);
}

PsiAccount* ContactListDragModel::getDropAccount(PsiAccount* account, const QModelIndex& parent) const
{
	Q_UNUSED(account);

	if (!parent.isValid())
		return 0;

	ContactListItem *item = static_cast<ContactListItem*>(parent.internalPointer());
	Q_ASSERT(item);
	if (!item)
		return 0;

	if (item->isContact()) {
		return item->contact()->account();
	}
	else if (item->isAccount()) {
		return item->account();
	}

	return 0;
}

QString ContactListDragModel::getDropGroupName(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return QString();

	ContactListItem *item = static_cast<ContactListItem*>(parent.internalPointer());
	Q_ASSERT(item);
	if (!item)
		return 0;

	if (item->isGroup()) {
		return item->name();
	}
	else if (item->isContact() && item->parent()->isGroup()) {
		return item->parent()->name();
	}

	return QString();
}

QString ContactListDragModel::sourceOperationsForContactGroup(const QString& groupName, PsiContact* contact) const
{
	Q_UNUSED(groupName);
	Q_UNUSED(contact);

	return QString();
}

QString ContactListDragModel::destinationOperationsForContactGroup(const QString& groupName, PsiContact* contact) const
{
	Q_UNUSED(groupName);
	Q_UNUSED(contact);

	return QString();
}

QModelIndexList ContactListDragModel::indexesFor(PsiContact* contact, QMimeData* contactSelection) const
{
	QModelIndexList indexes;
	if (contactSelection) {
		indexes += indexesFor(contactSelection);
	}
	if (indexes.isEmpty() && contact) {
		indexes += ContactListModel::indexesFor(contact);
	}
	return indexes;
}
