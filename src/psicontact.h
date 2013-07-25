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

#ifndef PSICONTACT_H
#define PSICONTACT_H

#include "contactlistitem.h"
#include "psicontactlist.h"
#include "xmpp_vcard.h"

class PsiIcon;
class PsiAccount;
class UserListItem;
class UserResourceList;
class YaProfile;

class PsiContact : public ContactListItem
{
	Q_OBJECT
private:
	PsiContact(const PsiContact&);
	PsiContact& operator=(const PsiContact&);

protected:
	PsiContact();

public:
	PsiContact(const UserListItem& u, PsiAccount* account);
	~PsiContact();

	PsiAccount* account() const;
	const UserListItem& userListItem() const;
	const UserResourceList& userResourceList() const;
	virtual void update(const UserListItem& u);

	bool isBlocked() const;
	virtual bool isSelf() const;
	virtual bool isAgent() const;
	virtual bool inList() const;
	virtual bool isPrivate() const;
	virtual bool noGroups() const;
	virtual bool authorized() const;
	virtual bool authorizesToSeeStatus() const;
	virtual bool askingForAuth() const;
	virtual bool isOnline() const;
	virtual bool isHidden() const;
	virtual bool isValid() const;

	virtual bool isAnimated() const;

	void activate();

	virtual void setEditing(bool editing);

	bool addAvailable() const;
	bool removeAvailable() const;
	bool authAvailable() const;
	bool blockAvailable() const;
#ifdef YAPSI
	bool isYaInformer() const;
	bool isYaJid();
	bool isYandexTeamJid();
	bool historyAvailable() const;
	bool moodNotificationsEnabled() const;
	void setMoodNotificationsEnabled(bool enabled);

	void showOnlineTemporarily();
	void setReconnectingState(bool reconnecting);

	void startDelayedMoodUpdate(int timeoutInSecs);
#endif

	virtual bool isFake() const;

	// reimplemented
	virtual ContactListModel::Type type() const;
	virtual const QString& name() const;
	virtual QString comparisonName() const;
	virtual void setName(const QString& name);
	virtual ContactListItemMenu* contextMenu(ContactListModel* model);
	virtual bool isEditable() const;
	virtual bool isDragEnabled() const;
	virtual bool compare(const ContactListItem* other) const;
	virtual bool isRemovable() const;

	virtual XMPP::Jid jid() const;
	virtual XMPP::Status status() const;
	virtual QString statusText() const;
	virtual QString toolTip() const;
	virtual QIcon picture() const;
	virtual QIcon alertPicture() const;

#ifdef YAPSI
	XMPP::VCard::Gender gender() const;
#endif
	void rereadVCard();

	bool groupOperationPermitted(const QString& oldGroupName, const QString& newGroupName) const;
	virtual QStringList groups() const;
	virtual void setGroups(QStringList);
	bool alerting() const;
	void setAlert(const PsiIcon* icon);
	void startAnim();
	bool find(const Jid& jid) const;
	// PsiContactList* contactList() const;

	static QString generalGroupName();
	static QString notInListGroupName();
	static QString hiddenGroupName();

protected:
	virtual bool shouldBeVisible() const;
	// virtual ContactListGroupItem* desiredParent() const;

public slots:
	virtual void receiveIncomingEvent();
	virtual void sendMessage();
	virtual void sendMessageTo(QString resource);
	virtual void openChat();
	virtual void openChatTo(QString resource);
#ifdef WHITEBOARDING
	virtual void openWhiteboard();
	virtual void openWhiteboardTo(QString resource);
#endif
	virtual void executeCommand(QString resource);
	virtual void openActiveChat(QString resource);
	virtual void sendFile();
	virtual void inviteToGroupchat(QString groupChat);
	virtual void toggleBlockedState();
	virtual void toggleBlockedStateConfirmation();
	virtual void rerequestAuthorizationFrom();
	virtual void removeAuthorizationFrom();
	virtual void remove();
	virtual void assignCustomPicture();
	virtual void clearCustomPicture();
	virtual void userInfo();
	virtual void history();
#ifdef YAPSI
	virtual void yaProfile();
	virtual void yaPhotos();
	virtual void yaEmail();
#endif
#ifdef YAPSI
	void moodUpdate();
#endif

	void stopAnim();

private slots:
	void avatarChanged(const Jid&);
	void vcardChanged(const Jid&);

	void blockContactConfirmation(const QString& id, bool confirmed);
	void blockContactConfirmationHelper(bool block);

signals:
	void alert();
	void anim();
	void updated();
	void groupsChanged();
#ifdef YAPSI
	void moodChanged(const QString&);
#endif

	/**
	 * This signal is emitted when PsiContact has entered its final
	 * destruction stage.
	 */
	void destroyed(PsiContact*);

private:
	class Private;
	Private *d;

	void addRemoveAuthBlockAvailable(bool* add, bool* remove, bool* auth, bool* block) const;
#ifdef YAPSI
	YaProfile getYaProfile() const;
#endif
};

#endif
