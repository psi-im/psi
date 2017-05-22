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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CONTACTLISTDRAGMODEL_H
#define CONTACTLISTDRAGMODEL_H

#include "contactlistmodel.h"

class ContactListItem;
class ContactListGroupItem;
class PsiContactGroup;
class PsiAccount;
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
		Operation(const QString &_groupFrom, const QString &_groupTo)
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

	void addOperation(PsiContact* contact, const QString &groupFrom, const QString &groupTo);
	QList<ContactOperation> operations() const;

	void removeAccidentalContactMoveOperations();

private:
	Action action_;
	QHash<PsiContact*, QList<Operation>> operations_;
};

class ContactListDragModel : public ContactListModel
{
	Q_OBJECT

public:
	ContactListDragModel(PsiContactList *contactList);

	// reimplemented
	Qt::DropActions supportedDragActions() const;
	Qt::DropActions supportedDropActions() const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	QStringList mimeTypes() const;
	QMimeData *mimeData(const QModelIndexList &indexes) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
	void renameGroup(ContactListItem *group, const QString &newName);

	QModelIndexList indexesFor(const QMimeData *data) const;
	QModelIndexList indexesFor(PsiContact *contact, QMimeData *contactSelection) const;

	bool supportsMimeDataOnIndex(const QMimeData *data, const QModelIndex &parent) const;

protected:
	enum OperationType {
		Operation_DragNDrop = 0,
		Operation_GroupRename
	};

	virtual PsiAccount *getDropAccount(PsiAccount *account, const QModelIndex &parent) const;
	virtual QString getDropGroupName(const QModelIndex &parent) const;

	QString sourceOperationsForContactGroup(const QString &groupName, PsiContact *contact) const;
	QString destinationOperationsForContactGroup(const QString &groupName, PsiContact *contact) const;
};

#endif
