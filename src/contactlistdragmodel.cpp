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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "contactlistdragmodel.h"

#include <QtAlgorithms>
#include <QTimer>

#include "psioptions.h"
#include "psiaccount.h"
#include "contactlistgroup.h"
#include "contactlistitemproxy.h"
#include "psicontactlist.h"
#include "psicontact.h"
#include "common.h"
#include "contactlistnestedgroup.h"
#include "contactlistaccountgroup.h"
#include "contactlistmodelselection.h"
#include "contactlistgroupcache.h"
#include "contactlistmodelupdater.h"
#include "contactlistgroup.h"
#include "contactlistspecialgroup.h"

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

ContactListDragModel::ContactListDragModel(PsiContactList* contactList)
	: ContactListModel(contactList)
{
	setSupportedDragActions(Qt::MoveAction | Qt::CopyAction);
}

ContactListModel* ContactListDragModel::clone() const
{
	return new ContactListDragModel(contactList());
}

Qt::DropActions ContactListDragModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

Qt::ItemFlags ContactListDragModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = ContactListModel::flags(index);

	ContactListItemProxy* item = static_cast<ContactListItemProxy*>(index.internalPointer());

	if (item && item->item()) {
		return f | Qt::ItemIsDropEnabled | (item->item()->isDragEnabled() ? Qt::ItemIsDragEnabled : f);
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
	QList<ContactListItemProxy*> items;

	foreach(QModelIndex index, indexes) {
		if (index.isValid()) {
			ContactListItemProxy* itemProxy = static_cast<ContactListItemProxy*>(index.internalPointer());
			if (!itemProxy)
				continue;

			items << itemProxy;
		}
	}

	return new ContactListModelSelection(items);
}

QModelIndexList ContactListDragModel::indexesFor(const QMimeData* data) const
{
	QModelIndexList result;
	ContactListModelSelection selection(data);
	if (!selection.haveRosterSelection() || !contactList())
		return result;

	foreach(ContactListModelSelection::Contact contact, selection.contacts()) {
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

		QList<ContactListGroup*> groups = groupCache()->groupsFor(psiContact);
		if (!contact.group.isEmpty()) {
			QList<ContactListGroup*> matched;
			foreach(ContactListGroup* group, groups) {
				if (group->fullName() == contact.group) {
					matched << group;
					break;
				}
			}

			if (!matched.isEmpty())
				groups = matched;
		}

		foreach(ContactListGroup* group, groups) {
			int contactIndex = group->indexOf(psiContact);
			if (contactIndex >= 0) {
				ContactListItemProxy* itemProxy = group->item(contactIndex);
				if (itemProxy) {
					QModelIndex index = itemProxyToModelIndex(itemProxy);
					if (index.isValid() && !result.contains(index))
						result << index;
				}
			}
		}
	}

	foreach(ContactListModelSelection::Group group, selection.groups()) {
		ContactListGroup* contactGroup = groupCache()->findGroup(group.fullName);
		QModelIndex index = groupToIndex(contactGroup);
		if (!result.contains(index))
			result << index;
	}

	foreach(ContactListModelSelection::Account account, selection.accounts()) {
		PsiAccount* acc = contactList()->getAccount(account.id);
		if (!acc)
			continue;
		ContactListAccountGroup* rootGroup = dynamic_cast<ContactListAccountGroup*>(this->rootGroup());
		if (!rootGroup)
			continue;
		ContactListGroup* accountGroup = rootGroup->findAccount(acc);
		QModelIndex index = groupToIndex(accountGroup);
		if (!result.contains(index))
			result << index;
	}

	return result;
}

bool ContactListDragModel::supportsMimeDataOnIndex(const QMimeData* data, const QModelIndex& parent) const
{
	if ((!groupsEnabled() && !accountsEnabled()) || !ContactListModelSelection(data).haveRosterSelection()) {
		return false;
	}

#if defined(YAPSI) and defined(USE_GENERAL_CONTACT_GROUP)
	// in YaPsi there's no accounts in roster, and when General group is disabled,
	// contacts could be placed directly at the top level
	if (!accountsEnabled() && !parent.isValid()) {
		return false;
	}
#endif

	{
		// disable dragging to special groups
		ContactListItemProxy* item = itemProxy(parent);
		ContactListGroup* group = dynamic_cast<ContactListGroup*>(item ? item->item() : 0);
		if (!group)
			group = item ? item->parent() : 0;
		if (group && !group->isEditable())
			return false;
	}

	foreach(QModelIndex index, indexesFor(data)) {
		if (index == parent) {
			return false;
		}

		// protection against dragging parent group inside its child
		ContactListItemProxy* item = itemProxy(index);
		Q_ASSERT(item);
		ContactListGroup* group = dynamic_cast<ContactListGroup*>(item->item());
		if (group) {
			ContactListItemProxy* item2 = itemProxy(parent);
			ContactListGroup* group2 = item2 ? item2->parent() : 0;
			if (group2 && group2->fullName().startsWith(group->fullName() + ContactListGroup::groupDelimiter())) {
				return false;
			}
		}

		PsiContact* contact = dynamic_cast<PsiContact*>(item->item());
		if (contact && accountsEnabled()) {
			// disable dragging to self contacts
			ContactListItemProxy* selfContactItem = itemProxy(parent);
			PsiContact* selfContact = selfContactItem? dynamic_cast<PsiContact*>(selfContactItem->item()) : 0;
			if (selfContact && selfContact->isSelf())
				return false;

			// disable dragging to different accounts and to self account
			QModelIndex accountIndex = parent;
			while (accountIndex.isValid() &&
			       ContactListModel::indexType(accountIndex) != ContactListModel::AccountType)
			{
				accountIndex = accountIndex.parent();
			}

			if (!accountIndex.isValid())
				return false;

			ContactListItemProxy* accountItem = itemProxy(accountIndex);
			Q_ASSERT(accountItem);
			ContactListAccountGroup* accountGroup = dynamic_cast<ContactListAccountGroup*>(accountItem->item());
			Q_ASSERT(accountGroup);
			return accountGroup &&
			       parent != accountIndex &&
			       accountGroup->account() == contact->account();
		}
	}

	return true;
}

void ContactListDragModel::addOperationsForGroupRename(const QString& currentGroupName, const QString& newGroupName, ContactListModelOperationList* operations) const
{
	ContactListGroup* group = groupCache()->findGroup(currentGroupName);
	if (group) {
		for (int i = 0; i < group->itemsCount(); ++i) {
			ContactListItemProxy* itemProxy = group->item(i);
			PsiContact* contact = 0;
			ContactListGroup* childGroup = 0;
			if ((contact = dynamic_cast<PsiContact*>(itemProxy->item()))) {
				operations->addOperation(contact,
				                         sourceOperationsForContactGroup(currentGroupName, contact),
				                         destinationOperationsForContactGroup(newGroupName, contact));
			}
#ifdef CONTACTLIST_NESTED_GROUPS
#error needs testing
			else if ((childGroup = dynamic_cast<ContactListGroup*>(itemProxy->item()))) {
				QString theName = childGroup->fullName().split(ContactListGroup::groupDelimiter()).last();
				QString newName = (newGroupName.isEmpty() ? "" : newGroupName + ContactListGroup::groupDelimiter()) + theName;
				addOperationsForGroupRename(childGroup->fullName(), newName, operations);
			}
#endif
		}
	}
}

bool ContactListDragModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	Q_UNUSED(row);
	Q_UNUSED(column);

	ContactListModelSelection selection(data);
	if (!selection.haveRosterSelection() || !contactList())
		return false;

	ContactListModelOperationList operations(action);

	foreach(ContactListModelSelection::Contact contact, selection.contacts()) {
		PsiAccount* account = contactList()->getAccount(contact.account);
		PsiContact* psiContact = account ? account->findContact(contact.jid) : 0;
		QString toGroup = getDropGroupName(parent);
		operations.addOperation(psiContact,
		                        contact.group,
		                        destinationOperationsForContactGroup(toGroup, psiContact));
	}

	foreach(ContactListModelSelection::Group group, selection.groups()) {
		QString parentGroupName = getDropGroupName(parent);
		if (parentGroupName.startsWith(group.fullName + ContactListGroup::groupDelimiter())) {
			qWarning("Dropping group to its descendant is not supported ('%s' -> '%s')", qPrintable(group.fullName), qPrintable(parentGroupName));
			continue;
		}

		if (parentGroupName == group.fullName)
			continue;

		// TODO: unify these two lines with the ones in operationsForGroupRename
		QString theName = group.fullName.split(ContactListGroup::groupDelimiter()).last();
		QString newName = (parentGroupName.isEmpty() ? "" : parentGroupName + ContactListGroup::groupDelimiter()) + theName;
		if (newName == group.fullName)
			continue;

		addOperationsForGroupRename(group.fullName, newName, &operations);
	}

	if (selection.isMultiSelection())
		operations.removeAccidentalContactMoveOperations();

#ifdef ENABLE_CL_DEBUG
	qWarning("*** dropMimeData: New operation list: ***");
	foreach(ContactListModelOperationList::ContactOperation contactOperation, operations.operations()) {
		QStringList groups;
		foreach(ContactListModelOperationList::Operation op, contactOperation.operations)
			groups += QString("'%1' -> '%2'").arg(op.groupFrom, op.groupTo);
		qWarning("\t'%s' (%s)", qPrintable(contactOperation.contact->jid().full()), qPrintable(groups.join(";")));
	}
#endif

	performContactOperations(operations, Operation_DragNDrop);

	return true;
}

void ContactListDragModel::renameGroup(ContactListGroup* group, const QString& newName)
{
	Q_ASSERT(group);
	ContactListModelOperationList operations(ContactListModelOperationList::Move);

	QStringList name = group->fullName().split(ContactListGroup::groupDelimiter());
	if (name.isEmpty())
		return;
	name.takeLast();
	if (!newName.isEmpty())
		name << newName;
	addOperationsForGroupRename(group->fullName(), name.join(ContactListGroup::groupDelimiter()), &operations);

	performContactOperations(operations, Operation_GroupRename);
}

QString ContactListDragModel::processContactSetGroupName(const QString& groupName) const
{
	if (accountsEnabled()) {
		QStringList split = groupName.split(ContactListGroup::groupDelimiter());
		split.takeFirst();
		return split.join(ContactListGroup::groupDelimiter());
	}

	return groupName;
}

QStringList ContactListDragModel::processContactSetGroupNames(const QStringList& groups) const
{
	QStringList result;
	foreach(const QString& g, groups) {
		result << processContactSetGroupName(g);
	}
	return result;
}

QStringList ContactListDragModel::processContactGetGroupNames(PsiContact* contact) const
{
	QStringList groups;

	if (contact) {
		QList<ContactListGroup*> list = groupCache()->groupsFor(contact);
		foreach(ContactListGroup* clg, list) {
			groups << clg->fullName();
		}
	}

	return groups;
}

void ContactListDragModel::performContactOperations(const ContactListModelOperationList& operations, OperationType operationType)
{
	QHash<ContactListGroup*, int> groupContactCount;

	foreach(ContactListModelOperationList::ContactOperation contactOperation, operations.operations()) {
		PsiContact* psiContact = contactOperation.contact;
		if (!psiContact) {
			qWarning("Only contact moving via drag'n'drop is supported for now.");
			continue;
		}

		// PsiAccount* dropAccount = getDropAccount(account, parent);
		// if (!dropAccount || dropAccount != account) {
		// 	qWarning("Dropping to different accounts is not supported for now.");
		// 	return;
		// }

		QStringList groups = processContactGetGroupNames(psiContact);

		foreach(ContactListModelOperationList::Operation op, contactOperation.operations) {
			if (operations.action() == ContactListModelOperationList::Move) {
				groups.removeAll(op.groupFrom);

				ContactListGroup* group = groupCache()->findGroup(op.groupFrom);
				if (group && groupContactCount.contains(group)) {
					groupContactCount[group] -= 1;
				}
				else if (group) {
					groupContactCount[group] = group->contactsCount() - 1;
				}
			}

			if (!groups.contains(op.groupTo)) {
				groups << op.groupTo;
			}
		}

		psiContact->setGroups(processContactSetGroupNames(groups));
	}

	contactOperationsPerformed(operations, operationType, groupContactCount);
}

void ContactListDragModel::contactOperationsPerformed(const ContactListModelOperationList& operations, OperationType operationType, const QHash<ContactListGroup*, int>& groupContactCount)
{
	Q_UNUSED(operations);
	Q_UNUSED(operationType);
	Q_UNUSED(groupContactCount);
}

// TODO: think of a way to merge this with performContactOperations
QList<PsiContact*> ContactListDragModel::removeIndexesHelper(const QMimeData* data, bool performRemove)
{
	QList<PsiContact*> result;
	QMap<PsiContact*, QStringList> toRemove;
	ContactListModelOperationList operations = removeOperationsFor(data);

	foreach(ContactListModelOperationList::ContactOperation contactOperation, operations.operations()) {
		PsiContact* psiContact = contactOperation.contact;
		if (!psiContact)
			continue;

		QStringList groups = psiContact->groups();

		foreach(ContactListModelOperationList::Operation op, contactOperation.operations) {
			groups.removeAll(op.groupFrom);
		}

		if (!groupsEnabled()) {
			groups.clear();
		}

		if (psiContact) {
			if (!result.contains(psiContact)) {
				result << psiContact;
			}
		}

		toRemove[psiContact] = groups;
	}

	if (performRemove && !toRemove.isEmpty()) {
		beginBulkUpdate();

		QMapIterator<PsiContact*, QStringList> it(toRemove);
		while (it.hasNext()) {
			it.next();
			PsiContact* psiContact = it.key();
			if (filterRemoveContact(psiContact, it.value()))
				continue;

			if (!psiContact)
				continue;

			QStringList groups = it.value();
			if (!groups.isEmpty())
				psiContact->setGroups(processContactSetGroupNames(groups));
			else
				psiContact->remove();
		}

		endBulkUpdate();
	}

	return result;
}

bool ContactListDragModel::filterRemoveContact(PsiContact* psiContact, const QStringList& newGroups)
{
	Q_UNUSED(psiContact);
	Q_UNUSED(newGroups);
	return false;
}

void ContactListDragModel::initializeModel()
{
	ContactListModel::initializeModel();
}

PsiAccount* ContactListDragModel::getDropAccount(PsiAccount* account, const QModelIndex& parent) const
{
	Q_UNUSED(account);

	if (!parent.isValid())
		return 0;

	ContactListItemProxy* item = static_cast<ContactListItemProxy*>(parent.internalPointer());
	PsiContact* contact = 0;

	if (item && (contact = dynamic_cast<PsiContact*>(item->item()))) {
		return contact->account();
	}
	// else if ((group = dynamic_cast<ContactListGroup*>(item))) {
	// 	return group->account();
	// }

	return 0;
}

QString ContactListDragModel::getDropGroupName(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return QString();

	ContactListItemProxy* item = static_cast<ContactListItemProxy*>(parent.internalPointer());
	ContactListGroup* group = 0;

	if (item) {
		if ((group = dynamic_cast<ContactListGroup*>(item->item()))) {
			return group->fullName();
		}
		else {
			Q_ASSERT(item->parent());
			return item->parent()->fullName();
		}
	}

	return QString();
}

QString ContactListDragModel::sourceOperationsForContactGroup(const QString& groupName, PsiContact* contact) const
{
	ContactListGroup* contactGroup = groupCache()->findGroup(groupName);
	ContactListSpecialGroup* specialGroup = contactGroup ? dynamic_cast<ContactListSpecialGroup*>(contactGroup) : 0;
	if (specialGroup) {
		return specialGroup->sourceOperationsForSpecialGroupContact(contact);
	}
	return processContactSetGroupName(groupName);
}

QString ContactListDragModel::destinationOperationsForContactGroup(const QString& groupName, PsiContact* contact) const
{
	ContactListGroup* contactGroup = groupCache()->findGroup(groupName);
	ContactListSpecialGroup* specialGroup = contactGroup ? dynamic_cast<ContactListSpecialGroup*>(contactGroup) : 0;
	if (specialGroup) {
		return specialGroup->destinationOperationsForSpecialGroupContact(contact);
	}
	return groupName;
}

ContactListModelOperationList ContactListDragModel::removeOperationsFor(const QMimeData* data) const
{
	ContactListModelOperationList operations(ContactListModelOperationList::Remove);
	ContactListModelSelection selection(data);

	if (!contactList())
		return operations;

	foreach(ContactListModelSelection::Contact contact, selection.contacts()) {
		PsiAccount* account = contactList()->getAccount(contact.account);
		PsiContact* psiContact = account ? account->findContact(contact.jid) : 0;

		operations.addOperation(psiContact,
		                        sourceOperationsForContactGroup(contact.group, psiContact),
		                        QString());
	}

	foreach(ContactListModelSelection::Group group, selection.groups()) {
		addOperationsForGroupRename(group.fullName, QString(), &operations);
	}

#ifdef ENABLE_CL_DEBUG
	qWarning("*** removeOperationsFor: New operation list: ***");
	foreach(ContactListModelOperationList::ContactOperation contactOperation, operations.operations()) {
		QStringList groups;
		foreach(ContactListModelOperationList::Operation op, contactOperation.operations)
			groups += QString("'%1' -> '%2'").arg(op.groupFrom, op.groupTo);
		qWarning("\t'%s' (%s)", qPrintable(contactOperation.contact->jid().full()), qPrintable(groups.join(";")));
	}
#endif

	return operations;
}

/**
 * Returns list of contacts that will be removed from roster by removeIndexes().
 * Contacts that will not be reported by this function will only lose some groups
 * they belong to.
 */
QList<PsiContact*> ContactListDragModel::contactsLostByRemove(const QMimeData* data) const
{
	return ((ContactListDragModel*)this)->removeIndexesHelper(data, false);
}

/**
 * Returns list of groups for specified contact which are going to be removed
 * when the specified data is removed.
 */
QStringList ContactListDragModel::contactGroupsLostByRemove(PsiContact* contact, const QMimeData* data) const
{
	Q_ASSERT(contact);
	Q_ASSERT(data);
	QStringList result;
	if (!contact || !data)
		return result;

	ContactListModelOperationList operations = removeOperationsFor(data);
	Q_ASSERT(operations.action() == ContactListModelOperationList::Remove);
	foreach(const ContactListModelOperationList::ContactOperation& op, operations.operations()) {
		if (op.contact != contact)
			continue;

		foreach(const ContactListModelOperationList::Operation& o, op.operations) {
			Q_ASSERT(o.groupTo.isEmpty());
			result << o.groupFrom;
		}
	}

	return result;
}

/**
 * Removes selected roster items from roster by either removing some of the
 * contacts' groups, or by removing them from roster altogether.
 */
void ContactListDragModel::removeIndexes(const QMimeData* data)
{
	removeIndexesHelper(data, true);
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
