/*
 * contactlistmodel.h - model of contact list
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

#include "abstracttreemodel.h"

#include <QModelIndex>
#include <QVariant>
#include <QHash>

class PsiAccount;
class PsiContact;
class PsiContactList;
class ContactListItem;

class ContactListModel : public AbstractTreeModel
{
	Q_OBJECT

public:
	enum {
		// generic
		TypeRole = Qt::UserRole,
		ActivateRole,
		ContactListItemRole,

		// contacts
		JidRole,
		PictureRole,
		StatusTextRole,
		StatusTypeRole,
		PresenceErrorRole,
		IsAgentRole,
		AuthorizesToSeeStatusRole,
		AskingForAuthRole,
		IsAlertingRole,
		AlertPictureRole,
		IsAnimRole,
		PhaseRole,
		MoodRole,
		ActivityRole,
		GeolocationRole,
		ClientRole,
		TuneRole,
		AvatarRole,
		IsMucRole,
		MucMessagesRole,
		BlockRole,
		IsSecureRole,

		// groups
		ExpandedRole,
		TotalItemsRole,
		FullGroupNameRole,
		OnlineContactsRole,
		TotalContactsRole,
		InternalGroupNameRole,
		SpecialGroupTypeRole,
		DisplayGroupRole,

		// accounts
		UsingSSLRole,
	};

	enum {
		NameColumn = 0
	};

	ContactListModel(PsiContactList *contactList);
	virtual ~ContactListModel();

	virtual PsiContactList *contactList() const;

	void invalidateLayout();

	bool groupsEnabled() const;
	void setGroupsEnabled(bool enabled);

	bool accountsEnabled() const;
	void setAccountsEnabled(bool enabled);

	bool showOffline() const;
	bool showSelf() const;
	bool showTransports() const;
	bool showHidden() const;
	QString contactSortStyle() const;

	void renameSelectedItem();

	PsiContact *contactFor(const QModelIndex &index) const;
	QModelIndexList indexesFor(const PsiContact *contact) const;

	// reimplemented
	QVariant data(const QModelIndex& index, int role) const;
	virtual int columnCount(const QModelIndex &parent) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	virtual bool setData(const QModelIndex &index, const QVariant &data, int role);

	ContactListItem *toItem(const QModelIndex &index) const;
    QModelIndex toModelIndex(ContactListItem *item) const;

signals:
	void showOfflineChanged();
	void showSelfChanged();
	void showTransportsChanged();
	void showHiddenChanged();
	void inPlaceRename();
	void contactSortStyleChanged();

public slots:
	void expanded(const QModelIndex &index);
	void collapsed(const QModelIndex &index);

protected slots:
	void destroyingContactList();

protected slots:
	void rosterRequestFinished();

private:
	void updateItem(ContactListItem *item);

	class Private;
	Private *d;

	friend class ContactListItem;
};
