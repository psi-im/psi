/*
 * contactlistdragmodel.h - ContactListModel with support for Drag'n'Drop operations
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

#ifndef CONTACTLISTDRAGMODEL_H
#define CONTACTLISTDRAGMODEL_H

#include "contactlistmodel.h"

class ContactListItem;
class ContactListGroupItem;
class PsiContactGroup;
class PsiAccount;
class ContactListModelSelection;
class PsiContact;

#include "xmpp_jid.h"

#include <QHash>

class ContactListModelOperationList
{
public:
	enum Action {
		Copy = 0,
		Move,
		Remove
	};

	struct Operation {
		Operation()
		{}
		Operation(const QString& _groupFrom, const QString& _groupTo)
			: groupFrom(_groupFrom)
			, groupTo(_groupTo)
		{}
		QString groupFrom;
		QString groupTo;
	};

	struct ContactOperation {
		PsiContact* contact;
		QList<Operation> operations;
	};

	ContactListModelOperationList(Action action);
	ContactListModelOperationList(Qt::DropAction action);

	Action action() const;

	void addOperation(PsiContact* contact, const QString& groupFrom, const QString& groupTo);
	QList<ContactOperation> operations() const;

	void removeAccidentalContactMoveOperations();

private:
	Action action_;
	QHash<PsiContact*, QList<Operation> > operations_;
};

class ContactListDragModel : public ContactListModel
{
	Q_OBJECT
public:
	ContactListDragModel(PsiContactList* contactList);

	// reimplemented
	Qt::DropActions supportedDropActions() const;
	Qt::ItemFlags flags(const QModelIndex& index) const;
	QStringList mimeTypes() const;
	QMimeData* mimeData(const QModelIndexList& indexes) const;
	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);
	virtual ContactListModel* clone() const;
	void renameGroup(ContactListGroup* group, const QString& newName);

	QModelIndexList indexesFor(const QMimeData* data) const;
	QModelIndexList indexesFor(PsiContact* contact, QMimeData* contactSelection) const;

	QList<PsiContact*> contactsLostByRemove(const QMimeData* data) const;
	void removeIndexes(const QMimeData* data);
	QStringList contactGroupsLostByRemove(PsiContact* contact, const QMimeData* data) const;

	bool supportsMimeDataOnIndex(const QMimeData* data, const QModelIndex& parent) const;

protected:
	enum OperationType {
		Operation_DragNDrop = 0,
		Operation_GroupRename
	};

	// reimplemented
	virtual bool filterRemoveContact(PsiContact* psiContact, const QStringList& newGroups);
	virtual void initializeModel();

	virtual PsiAccount* getDropAccount(PsiAccount* account, const QModelIndex& parent) const;
	virtual QString getDropGroupName(const QModelIndex& parent) const;
	virtual void contactOperationsPerformed(const ContactListModelOperationList& operations, OperationType operationType, const QHash<ContactListGroup*, int>& groupContactCount);

	QString processContactSetGroupName(const QString& groupName) const;
	QStringList processContactSetGroupNames(const QStringList& groups) const;
	QStringList processContactGetGroupNames(PsiContact* contact) const;
	QString sourceOperationsForContactGroup(const QString& groupName, PsiContact* contact) const;
	QString destinationOperationsForContactGroup(const QString& groupName, PsiContact* contact) const;

private:
	QList<PsiContact*> removeIndexesHelper(const QMimeData* data, bool performRemove);
	ContactListModelOperationList removeOperationsFor(const QMimeData* data) const;
	void addOperationsForGroupRename(const QString& currentGroupName, const QString& newGroupName, ContactListModelOperationList* operations) const;
	void performContactOperations(const ContactListModelOperationList& operations, OperationType operationType);
};

#endif
