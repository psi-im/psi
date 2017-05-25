/*
 * psicontact.h - PsiContact
 * Copyright (C) 2008  Yandex LLC (Michail Pishchagin)
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

#include "psicontactlist.h"
#include "xmpp_vcard.h"
#include "contactlistitemmenu.h"

#include <QObject>

class PsiIcon;
class PsiAccount;
class UserListItem;
class UserResourceList;

class PsiContact : public QObject
{
	Q_OBJECT

private:
	PsiContact(const PsiContact&);
	PsiContact& operator=(const PsiContact&);

protected:
	PsiContact();

public:
	PsiContact(const UserListItem& u, PsiAccount* account, bool isSelf = false);
	~PsiContact();

	PsiAccount* account() const;
	const UserListItem& userListItem() const;
	const UserResourceList& userResourceList() const;
	void update(const UserListItem& u);

	bool isBlocked() const;
	bool isSelf() const;
	bool isAgent() const;
	bool inList() const;
	bool isPrivate() const;
	bool isConference() const;
	bool noGroups() const;
	bool authorized() const;
	bool authorizesToSeeStatus() const;
	bool askingForAuth() const;
	bool isOnline() const;
	bool isHidden() const;
	bool isValid() const;

	bool isAnimated() const;

	bool isActiveContact() const;

	void activate();

	bool isAlwaysVisible() const;
	void setAlwaysVisible(bool visible);

	bool addAvailable() const;
	bool removeAvailable() const;
	bool authAvailable() const;
	bool blockAvailable() const;

	bool isFake() const;

	// reimplemented
	const QString& name() const;
	QString comparisonName() const;
	void setName(const QString& name);
	ContactListItemMenu* contextMenu(ContactListModel* model);
	bool isEditable() const;
	bool isDragEnabled() const;
//	bool compare(const ContactListItem* other) const;
	bool isRemovable() const;

	XMPP::Jid jid() const;
	XMPP::Status status() const;
	QString statusText() const;
	QString toolTip() const;
	QIcon picture() const;
	QIcon alertPicture() const;

	void rereadVCard();

	bool groupOperationPermitted(const QString& oldGroupName, const QString& newGroupName) const;
	QStringList groups() const;
	void setGroups(QStringList);
	bool alerting() const;
	void setAlert(const PsiIcon* icon);
	void startAnim();
	bool find(const Jid& jid) const;
	// PsiContactList* contactList() const;

	static QString generalGroupName();
	static QString notInListGroupName();
	static QString hiddenGroupName();

protected:
	bool shouldBeVisible() const;
	// ContactListGroupItem* desiredParent() const;

public slots:
	void receiveIncomingEvent();
	void sendMessage();
	void sendMessageTo(QString resource);
	void openChat();
	void openChatTo(QString resource);
#ifdef WHITEBOARDING
	void openWhiteboard();
	void openWhiteboardTo(QString resource);
#endif
	void executeCommand(QString resource);
	void openActiveChat(QString resource);
	void sendFile();
	void inviteToGroupchat(QString groupChat);
	void toggleBlockedState();
	void toggleBlockedStateConfirmation();
	void rerequestAuthorizationFrom();
	void removeAuthorizationFrom();
	void remove();
	void clearCustomPicture();
	void userInfo();
	void history();

	void stopAnim();

private slots:
	void avatarChanged(const Jid&);
	void vcardChanged(const Jid&);

	void blockContactConfirmation(const QString& id, bool confirmed);
	void blockContactConfirmationHelper(bool block);

	void updateStatus();

signals:
	void alert();
	void anim();
	void updated();
	void groupsChanged();

	/**
	 * This signal is emitted when PsiContact has entered its final
	 * destruction stage.
	 */
	void destroyed(PsiContact*);

private:
	class Private;
	Private *d;

	void addRemoveAuthBlockAvailable(bool* add, bool* remove, bool* auth, bool* block) const;
};
