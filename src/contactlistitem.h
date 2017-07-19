/*
 * contactlistitem.h - base class for contact list items
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

#pragma once

#include "abstracttreeitem.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QPointer>

class PsiContact;
class PsiAccount;
class ContactListModel;
class ContactListItem;
class ContactListItemMenu;

typedef QList<ContactListItem*> ContactListItemList;

class ContactListItem : public AbstractTreeItem
{
public:
	enum class Type {
		InvalidType = 0,
		RootType    = 1,
		AccountType = 2,
		GroupType   = 3,
		ContactType = 4
	};

	enum class SpecialGroupType {
		GeneralSpecialGroupType         = 0,
		NoneSpecialGroupType            = 1,
		MucPrivateChatsSpecialGroupType = 2,
		ConferenceSpecialGroupType      = 3,
		NotInListSpecialGroupType       = 4,
		TransportsSpecialGroupType      = 5
	};

	ContactListItem(ContactListModel *model, Type type = Type::InvalidType, SpecialGroupType specialGropType = SpecialGroupType::NoneSpecialGroupType);
	~ContactListItem();

	ContactListModel *model() const;

	Type type() const;
	SpecialGroupType specialGroupType() const;

	bool isRoot() const;
	bool isAccount() const;
	bool isGroup() const;
	bool isContact() const;

	QString name() const;
	void setName(const QString& name);
	QString internalName() const;

	bool isEditable() const;
	bool isDragEnabled() const;
	bool isRemovable() const;

	bool isExpandable() const;
	bool expanded() const;
	void setExpanded(bool expanded);

	ContactListItemMenu *contextMenu();

	bool isFixedSize() const;

	bool lessThan(const ContactListItem* other) const;

	bool editing() const;
	void setEditing(bool editing);

	void setContact(PsiContact *contact);
	PsiContact *contact() const;

	void setAccount(PsiAccount *account);
	PsiAccount *account() const;

	bool shouldBeVisible() const;

	void setHidden(bool hidden);
	bool isHidden() const;

	QList<PsiContact*> contacts();

	ContactListItem *findAccount(PsiAccount *account);
	ContactListItem *findGroup(const QString &groupName);
	ContactListItem *findGroup(SpecialGroupType specialGroupType);
	ContactListItem *findContact(PsiContact *contact);

	void setValue(int role, const QVariant &value);
	QVariant value(int role) const;

	void updateContactsCount() const;

	QList<ContactListItem*> allChildren() const;

	// fake reimplemented
	inline ContactListItem *parent() const { return static_cast<ContactListItem*>(AbstractTreeItem::parent()); }
	inline ContactListItem *child(int row) const { return static_cast<ContactListItem*>(AbstractTreeItem::child(row)); }

private:
	ContactListModel *_model;
	Type _type;
	SpecialGroupType _specialGroupType;
	bool _editing;
	bool _selfValid; // hack hack! just to find one crash in roster. remove this and QPointer wrapper below when fixed
	QPointer<PsiContact> _contact;
	PsiAccount *_account;
	bool _expanded;
	QString _internalName;
	QString _displayName;
	mutable int _totalContacts;
	mutable int _onlineContacts;
	mutable bool _shouldBeVisible;
	bool _hidden;
};

Q_DECLARE_METATYPE(ContactListItem*)
Q_DECLARE_METATYPE(ContactListItem::Type)
