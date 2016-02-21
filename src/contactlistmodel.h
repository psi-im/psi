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

#ifndef CONTACTLISTMODEL_H
#define CONTACTLISTMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QHash>
#include <QPointer>

class PsiAccount;
class PsiContact;
class PsiContactList;
class ContactListItem;
class ContactListGroup;
class ContactListAccountGroup;
class ContactListItemProxy;
class ContactListGroupState;
class ContactListGroupCache;
class ContactListModelUpdater;

// #define ENABLE_CL_DEBUG
#ifdef ENABLE_CL_DEBUG
#	define CL_DEBUG(...) qWarning(__VA_ARGS__)
#else
#	define CL_DEBUG(...) /* noop */
#endif

class ContactListModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum Type {
		InvalidType = 0,
		ContactType = 1,
		GroupType   = 2,
		AccountType = 3
	};

	static ContactListModel::Type indexType(const QModelIndex& index);
	static bool isGroupType(const QModelIndex& index);

	enum {
		// generic
		TypeRole = Qt::UserRole + 0,
		ActivateRole = Qt::UserRole + 1,

		// contacts
		JidRole = Qt::UserRole + 2,
		PictureRole = Qt::UserRole + 3,
		StatusTextRole = Qt::UserRole + 4,
		StatusTypeRole = Qt::UserRole + 5,
		PresenceErrorRole = Qt::UserRole + 6,
		IsAgentRole = Qt::UserRole + 7,
		AuthorizesToSeeStatusRole = Qt::UserRole + 8,
		AskingForAuthRole = Qt::UserRole + 9,
		IsAlertingRole = Qt::UserRole + 10,
		AlertPictureRole = Qt::UserRole + 11,
		IsAnimRole = Qt::UserRole + 21,
		PhaseRole = Qt::UserRole + 22,
		MoodRole = Qt::UserRole + 23,
		ActivityRole = Qt::UserRole + 24,
		GeolocationRole = Qt::UserRole + 25,
		ClientRole = Qt::UserRole + 26,
		TuneRole = Qt::UserRole + 27,
		AvatarRole = Qt::UserRole + 28,
		IsMucRole = Qt::UserRole + 29,
		MucMessagesRole = Qt::UserRole + 30,

		// groups
		ExpandedRole = Qt::UserRole + 12,
		TotalItemsRole = Qt::UserRole + 13,
		FullGroupNameRole = Qt::UserRole + 14,
		OnlineContactsRole = Qt::UserRole + 15,
		TotalContactsRole = Qt::UserRole + 16,
		InternalGroupNameRole = Qt::UserRole + 17,
		SpecialGroupTypeRole = Qt::UserRole + 18,

		// accounts
		UsingSSLRole = Qt::UserRole + 19,

#ifdef YAPSI
		GenderRole = Qt::UserRole + 20,
#endif
	};

	enum {
		NameColumn = 0
	};

	ContactListModel(PsiContactList* contactList);
	virtual ~ContactListModel();

	virtual PsiContactList* contactList() const;

	void invalidateLayout();
	ContactListItemProxy* modelIndexToItemProxy(const QModelIndex& index) const;
	QModelIndex itemProxyToModelIndex(ContactListItemProxy* item) const;
	QModelIndex itemProxyToModelIndex(ContactListItemProxy* item, int index) const;
	QModelIndex groupToIndex(ContactListGroup* group) const;

	virtual ContactListModel* clone() const = 0;
	void contactListItemProxyCreated(ContactListItemProxy* proxy);

	bool groupsEnabled() const;
	void setGroupsEnabled(bool enabled);
	void storeGroupState(const QString& id);

	bool accountsEnabled() const;
	void setAccountsEnabled(bool enabled);

	bool showOffline() const;
	bool showSelf() const;
	bool showTransports() const;
	bool showHidden() const;
	bool hasContacts(bool onlineOnly) const;
	QString contactSortStyle() const;

	int groupOrder(const QString& groupFullName) const;
	void setGroupOrder(const QString& groupFullName, int order);

	void renameSelectedItem();

	PsiAccount* account(const QModelIndex& index) const;
	ContactListGroupState* groupState() const;
	ContactListGroupCache* groupCache() const;
	void updaterCommit();

	bool updatesEnabled() const;
	void setUpdatesEnabled(bool updatesEnabled);

	virtual void renameGroup(ContactListGroup* group, const QString& newName) = 0;

	PsiContact* contactFor(const QModelIndex& index) const;
	QModelIndexList indexesFor(PsiContact* contact) const;

	virtual QVariant contactListItemData(const ContactListItem* item, int role) const;
	virtual QVariant contactData(const PsiContact* contact, int role) const;
	virtual QVariant contactGroupData(const ContactListGroup* group, int role) const;
	virtual QVariant accountData(const ContactListAccountGroup* account, int role) const;

	// reimplemented
	QVariant data(const QModelIndex& index, int role) const;
	QVariant itemData(const ContactListItem* item, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const;
	virtual QModelIndex parent(const QModelIndex& index) const;
	virtual int rowCount(const QModelIndex& parent) const;
	virtual int columnCount(const QModelIndex& parent) const;
	Qt::ItemFlags flags(const QModelIndex& index) const;
	virtual bool setData(const QModelIndex&, const QVariant&, int role);
	virtual bool hasChildren(const QModelIndex& index);

public:
	void itemAboutToBeInserted(ContactListGroup* group, int index);
	void insertedItem(ContactListGroup* group, int index);
	void itemAboutToBeRemoved(ContactListGroup* group, int index);
	void removedItem(ContactListGroup* group, int index);
	void updatedItem(ContactListItemProxy* item);
	void updatedGroupVisibility(ContactListGroup* group);

signals:
	void showOfflineChanged();
	void showSelfChanged();
	void showTransportsChanged();
	void showHiddenChanged();
	void inPlaceRename();
	void contactSortStyleChanged();

	void contactAlert(const QModelIndex&);

public slots:
	void expanded(const QModelIndex&);
	void collapsed(const QModelIndex&);

protected slots:
	void addContact(PsiContact*);
	void removeContact(PsiContact*);

	void contactAlert(PsiContact*);
	virtual void contactAnim(PsiContact*);
	void contactUpdated(PsiContact*);
	void contactGroupsChanged(PsiContact*);

	void destroyingContactList();
	void orderChanged();

protected slots:
	void beginBulkUpdate();
	void endBulkUpdate();
	void rosterRequestFinished();

protected:
	virtual QList<PsiContact*> additionalContacts() const;
	ContactListModelUpdater* updater() const;
	ContactListGroup* rootGroup() const;

private:
	bool groupsEnabled_;
	bool accountsEnabled_;
	PsiContactList* contactList_;
	ContactListModelUpdater* updater_;
	ContactListGroup* rootGroup_;
	int bulkUpdateCount_;
	bool doResetAfterBulkUpdate_;
	bool doLayoutUpdateAfterBulkUpdate_;
	bool emitDeltaSignals_;
	ContactListGroupState* groupState_;
	ContactListGroupCache* groupCache_;
	QHash<ContactListItemProxy*, QPointer<ContactListItemProxy> > contactListItemProxyHash_;

protected:
	virtual QStringList filterContactGroups(QStringList groups) const;
	virtual ContactListGroup* createRootGroup();
	virtual void initializeModel();
	ContactListItemProxy* itemProxy(const QModelIndex& index) const;

	void addContact(PsiContact* contact, QStringList contactGroups);
	void contactGroupsChanged(PsiContact* contact, QStringList contactGroups);
};

#endif
