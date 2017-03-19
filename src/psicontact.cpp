/*
 * psicontact.cpp - PsiContact
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

#include "psicontact.h"

#include <QFileDialog>
#include <QTimer>

#include "avatars.h"
#include "common.h"
#ifndef NEWCONTACTLIST
# include "contactview.h"
#endif
#include "iconset.h"
#include "jidutil.h"
#include "profiles.h"
#include "psiaccount.h"
#include "psicontactmenu.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "userlist.h"
#include "userlist.h"
#include "alertable.h"
#include "avatars.h"
#ifdef YAPSI
#include "yaprivacymanager.h"
#include "yacommon.h"
#include "yaprofile.h"
#include "yatoastercentral.h"
#else
#include "psiprivacymanager.h"
#endif
#include "contactlistgroup.h"
#include "desktoputil.h"
#include "vcardfactory.h"
#include "psicon.h"
#include "psicontactlist.h"
#ifdef YAPSI
#include "yarostertooltip.h"
#endif

static const int statusTimerInterval = 5000;

class PsiContact::Private : public Alertable
{
	Q_OBJECT
public:
	Private(PsiContact* contact)
		: account_(0)
		, statusTimer_(0)
		, isValid_(true)
		, isAnimated_(false)
		, isAlwaysVisible_(false)
		, contact_(contact)
#ifdef YAPSI
		, gender_(XMPP::VCard::UnknownGender)
		, genderCached_(false)
#endif
	{
		oldStatus_ = XMPP::Status(XMPP::Status::Offline);
#ifdef YAPSI
		showOnlineTemporarily_ = false;
		reconnecting_ = false;

		delayedMoodUpdateTimer_ = new QTimer(this);
		delayedMoodUpdateTimer_->setInterval(60 * 1000);
		delayedMoodUpdateTimer_->setSingleShot(true);
		connect(delayedMoodUpdateTimer_, SIGNAL(timeout()), contact, SLOT(moodUpdate()));
#endif

		statusTimer_ = new QTimer(this);
		statusTimer_->setInterval(statusTimerInterval);
		statusTimer_->setSingleShot(true);
		connect(statusTimer_, SIGNAL(timeout()), SLOT(updateStatus()));

		animTimer_ = new QTimer(this);
		animTimer_->setInterval(5000);
		animTimer_->setSingleShot(true);
		connect(animTimer_, SIGNAL(timeout()), contact, SLOT(stopAnim()));
	}

	~Private()
	{
	}

	PsiAccount* account_;
	QTimer* statusTimer_;
	QTimer* animTimer_;
	UserListItem u_;
	QString name_;
	Status status_;
	Status oldStatus_;
	bool isValid_;
	bool isAnimated_;
	bool isAlwaysVisible_;
#ifdef YAPSI
	bool showOnlineTemporarily_;
	bool reconnecting_;
	QTimer* delayedMoodUpdateTimer_;
	XMPP::VCard::Gender gender_;
	bool genderCached_;
#endif

	XMPP::Status status(const UserListItem& u) const
	{
		XMPP::Status status = XMPP::Status(XMPP::Status::Offline);
		if (u.priority() != u.userResourceList().end())
			status = (*u.priority()).status();
		return status;
	}

	/**
	 * Returns userHost of the base jid combined with \param resource.
	 */
	Jid jidForResource(const QString& resource)
	{
		return u_.jid().withResource(resource);
	}

	void setStatus(XMPP::Status status)
	{
		status_ = status;
#ifdef YAPSI
		reconnecting_ = false;
#endif
		if (account_ && !account_->notifyOnline())
			oldStatus_ = status_;
		else
			statusTimer_->start();
	}

private slots:
	void updateStatus()
	{
#ifdef YAPSI
		showOnlineTemporarily_ = false;
#endif
		oldStatus_ = status_;
		emit contact_->updated();
	}

private:
	PsiContact* contact_;
};

/**
 * Creates new PsiContact.
 */
PsiContact::PsiContact(const UserListItem& u, PsiAccount* account)
	: ContactListItem(account)
{
	d = new Private(this);
	d->account_ = account;
	if (d->account_) {
		connect(d->account_->avatarFactory(), SIGNAL(avatarChanged(const Jid&)), SLOT(avatarChanged(const Jid&)));
	}
	connect(VCardFactory::instance(), SIGNAL(vcardChanged(const Jid&)), SLOT(vcardChanged(const Jid&)));
	update(u);

	// updateParent();
}

PsiContact::PsiContact()
	: ContactListItem(0)
{
	d = new Private(this);
	d->account_ = 0;
}

/**
 * Destructor.
 */
PsiContact::~PsiContact()
{
	d->isValid_ = false;
	emit destroyed(this);
	delete d;
}

/**
 * Returns account to which a contact belongs.
 */
PsiAccount* PsiContact::account() const
{
	return d->account_;
}

/**
 * TODO: Think of ways to remove this function.
 */
const UserListItem& PsiContact::userListItem() const
{
	return d->u_;
}

/**
 * This function should be called only by PsiAccount to update
 * PsiContact's current state.
 */
void PsiContact::update(const UserListItem& u)
{
	d->u_ = u;
	Status status = d->status(d->u_);

	d->setStatus(status);

	if (isAgent()) {
		d->u_.setGroups(QStringList());
	}
	emit updated();
	emit groupsChanged();
}

#ifdef YAPSI
void PsiContact::startDelayedMoodUpdate(int timeoutInSecs)
{
	d->delayedMoodUpdateTimer_->setInterval((timeoutInSecs + 1) * 1000);
	d->delayedMoodUpdateTimer_->start();
}

void PsiContact::moodUpdate()
{
	Status status = d->status(d->u_);

	QString newMood = Ya::processMood(d->u_.yaMood(), status.status(), status.type());

	if (status.isAvailable() &&
	    // !status.status().isEmpty() &&
	    newMood != d->u_.yaMood() &&
	    !d->u_.isSelf() &&
	    !isYaInformer())
	{
		// qWarning("mood changed: %s, '%s' (%s)", qPrintable(jid().full()), qPrintable(status.status()), qPrintable(d->u_.yaMood()));
		emit moodChanged(newMood);
	}
}
#endif

/**
 * Triggers default action.
 */
void PsiContact::activate()
{
	if (!account())
		return;
	account()->actionDefault(jid());
}

ContactListModel::Type PsiContact::type() const
{
	return ContactListModel::ContactType;
}

/**
 * Returns contact's display name.
 */
const QString& PsiContact::name() const
{
	d->name_ = JIDUtil::nickOrJid(d->u_.name(), jid().full());
	return d->name_;
}

QString PsiContact::comparisonName() const
{
	return name() + jid().full() + account()->name();
}

#ifdef YAPSI
XMPP::VCard::Gender PsiContact::gender() const
{
	if (!d->genderCached_)
		const_cast<PsiContact*>(this)->vcardChanged(jid());
	return d->gender_;
}
#endif

/**
 * Returns contact's XMPP address.
 */
XMPP::Jid PsiContact::jid() const
{
	return d->u_.jid();
}

/**
 * Returns contact's status.
 */
Status PsiContact::status() const
{
#ifdef YAPSI
	if (isBlocked()) {
		return XMPP::Status(XMPP::Status::Blocked);
	}

	if (d->reconnecting_) {
		return XMPP::Status(XMPP::Status::Reconnecting);
	}

	if (!authorizesToSeeStatus()) {
		return XMPP::Status(XMPP::Status::NotAuthorizedToSeeStatus);
	}
#endif
	return d->status_;
}

QString PsiContact::statusText() const
{
	if (d->u_.priority() == d->u_.userResourceList().end()) {
		return d->u_.lastUnavailableStatus().status();
	}
	return d->status_.status();
}

/**
 * Returns tool tip text for contact in HTML format.
 */
QString PsiContact::toolTip() const
{
	return d->u_.makeTip(true, false);
}

/**
 * Returns contact's avatar picture.
 */
QIcon PsiContact::picture() const
{
	if (!account())
		return QIcon();
	return account()->avatarFactory()->getAvatar(jid().bare());
}

/**
 * Creates a menu with actions for this contact.
 */
ContactListItemMenu* PsiContact::contextMenu(ContactListModel* model)
{
	if (!account())
		return 0;
	return new PsiContactMenu(this, model);
}

bool PsiContact::isFake() const
{
	return false;
}

/**
 * Returns true if user could modify (i.e. rename/change group/remove from
 * contact list) this contact on server.
 */
bool PsiContact::isEditable() const
{
	if (!account())
		return false;
	return account()->isAvailable() && inList();
}

bool PsiContact::isDragEnabled() const
{
	return isEditable()
	       && !isPrivate()
	       && !isAgent()
	       && !isConference();
}

/**
 * This function should be invoked when contact is being renamed by
 * user. \param name specifies new name. PsiContact is responsible
 * for the actual roster update.
 */
void PsiContact::setName(const QString& name)
{
	if (account())
		account()->actionRename(jid(), name);
}

QString PsiContact::generalGroupName()
{
	return tr("General");
}

QString PsiContact::notInListGroupName()
{
	return tr("Not in list");
}

QString PsiContact::hiddenGroupName()
{
	return tr("Hidden");
}

static const QString globalGroupDelimiter = "::";
static const QString accountGroupDelimiter = "::";

/**
 * Processes the list of groups to the result ready to be fed into
 * ContactListModel. All account-specific group delimiters are replaced
 * in favor of the global Psi one.
 */
QStringList PsiContact::groups() const
{
	QStringList result;
	if (!account())
		return result;

	if (!inList()) {
		result << QString();
	}
	else if (d->u_.groups().isEmpty()) {
		// empty group name means that the contact should be added
		// to the 'General' group or no group at all
// #ifdef USE_GENERAL_CONTACT_GROUP
// 		result << generalGroupName();
// #else
		result << QString();
// #endif
	}
	else {
		foreach(QString group, d->u_.groups()) {
			QString groupName = group.split(accountGroupDelimiter).join(globalGroupDelimiter);
// #ifdef USE_GENERAL_CONTACT_GROUP
// 			if (groupName.isEmpty()) {
// 				groupName = generalGroupName();
// 			}
// #endif
			if (!result.contains(groupName)) {
				result << groupName;
			}
		}
	}

	return result;
}

/**
 * Sets contact's groups to be \param newGroups. All associations to all groups
 * it belonged to prior to calling this function are lost. \param newGroups
 * becomes the only groups of the contact. Note: \param newGroups should be passed
 * with global Psi group delimiters.
 */
void PsiContact::setGroups(QStringList newGroups)
{
	if (!account())
		return;
	QStringList newAccountGroups;
	foreach(QString group, newGroups) {
		QString groupName = group.split(globalGroupDelimiter).join(accountGroupDelimiter);
// #ifdef USE_GENERAL_CONTACT_GROUP
// 		if (groupName == generalGroupName()) {
// 			groupName = QString();
// 		}
// #endif
		newAccountGroups << groupName;
	}

	if (newAccountGroups.count() == 1 && newAccountGroups.first().isEmpty())
		newAccountGroups.clear();

	account()->actionGroupsSet(jid(), newAccountGroups);
}

bool PsiContact::groupOperationPermitted(const QString& oldGroupName, const QString& newGroupName) const
{
	Q_UNUSED(oldGroupName);
	Q_UNUSED(newGroupName);
	return true;
}

bool PsiContact::isRemovable() const
{
	foreach(QString group, groups()) {
		if (!groupOperationPermitted(group, QString()))
			return false;
	}
	if (!inList()) {
		return true;
	}
	return account()->isAvailable();
}

/**
 * Returns true if contact currently have an alert set.
 */
bool PsiContact::alerting() const
{
	return d->alerting();
}

QIcon PsiContact::alertPicture() const
{
	Q_ASSERT(alerting());
	if (!alerting())
		return QIcon();
	return d->currentAlertFrame();
}

/**
 * Sets alert icon for contact. Pass null pointer in order to clear alert.
 */
void PsiContact::setAlert(const PsiIcon* icon)
{
#ifndef YAPSI
	d->setAlert(icon);
	// updateParent();
#endif
	emit alert();
}

void PsiContact::startAnim()
{
	d->isAnimated_ = true;
	d->animTimer_->start();

	anim();
}

void PsiContact::stopAnim()
{
	d->isAnimated_ = false;
	emit anim();
}

bool PsiContact::isAnimated() const
{
	return d->isAnimated_;
}

/**
 * Returns true if there is opened chat with this contact
 */
bool PsiContact::isActiveContact() const
{
	if(isConference()) {
		return false;
	}
	return account()->findChatDialog(jid(), isPrivate() ? true : false);
}

/**
 * Contact should always be visible if it's alerting.
 */
bool PsiContact::shouldBeVisible() const
{
#ifndef YAPSI
	if (d->alerting())
		return true;
#endif
	return false;
	// return ContactListItem::shouldBeVisible();
}

bool PsiContact::isAlwaysVisible() const
{
	return d->isAlwaysVisible_;
}

void PsiContact::setAlwaysVisible(bool visible)
{
	d->isAlwaysVisible_ = visible;
	account()->updateAlwaysVisibleContact(this);
	emit updated();
}

/**
 * Standard desired parent.
 */
// ContactListGroupItem* PsiContact::desiredParent() const
// {
// 	return ContactListContact::desiredParent();
// }

/**
 * Returns true if this contact is the one for the \param jid. In case
 * if reimplemented in metacontact-enabled subclass, it could match
 * several different jids.
 */
bool PsiContact::find(const Jid& jid) const
{
	return this->jid().compare(jid);
}

/**
 * This behaves just like ContactListContact::contactList(), but statically
 * casts returned value to PsiContactList.
 */
// PsiContactList* PsiContact::contactList() const
// {
// 	return static_cast<PsiContactList*>(ContactListContact::contactList());
// }

const UserResourceList& PsiContact::userResourceList() const
{
	return d->u_.userResourceList();
}

void PsiContact::receiveIncomingEvent()
{
	if (account())
		account()->actionRecvEvent(jid());
}

void PsiContact::sendMessage()
{
	if (account())
		account()->actionSendMessage(jid());
}

void PsiContact::sendMessageTo(QString resource)
{
	if (account())
		account()->actionSendMessage(d->jidForResource(resource));
}

void PsiContact::openChat()
{
	if (account())
		account()->actionOpenChat(jid());
}

void PsiContact::openChatTo(QString resource)
{
	if (account())
		account()->actionOpenChatSpecific(d->jidForResource(resource));
}

#ifdef WHITEBOARDING
void PsiContact::openWhiteboard()
{
	if (account())
		account()->actionOpenWhiteboard(jid());
}

void PsiContact::openWhiteboardTo(QString resource)
{
	if (account())
		account()->actionOpenWhiteboardSpecific(d->jidForResource(resource));
}
#endif

void PsiContact::executeCommand(QString resource)
{
	if (account())
		account()->actionExecuteCommandSpecific(d->jidForResource(resource), "");
}

void PsiContact::openActiveChat(QString resource)
{
	if (account())
		account()->actionOpenChatSpecific(d->jidForResource(resource));
}

void PsiContact::sendFile()
{
	if (account())
		account()->actionSendFile(jid());
}

void PsiContact::inviteToGroupchat(QString groupChat)
{
	if (account())
		account()->actionInvite(jid(), groupChat);
}

void PsiContact::toggleBlockedState()
{
	if (!account())
		return;

// FIXME
#ifdef YAPSI
	YaPrivacyManager* privacyManager = dynamic_cast<YaPrivacyManager*>(account()->privacyManager());
	Q_ASSERT(privacyManager);

	bool blocked = privacyManager->isContactBlocked(jid());
	if (!blocked) {
		emit YaRosterToolTip::instance()->blockContact(this, 0);
	}
	else {
		emit YaRosterToolTip::instance()->unblockContact(this, 0);
	}
#endif
}

void PsiContact::toggleBlockedStateConfirmation()
{
	if (!account())
		return;

// FIXME
#ifdef YAPSI
	YaPrivacyManager* privacyManager = dynamic_cast<YaPrivacyManager*>(account()->privacyManager());
	Q_ASSERT(privacyManager);

	bool blocked = privacyManager->isContactBlocked(jid());
	blockContactConfirmationHelper(!blocked);
#else
	PsiPrivacyManager* privacyManager = dynamic_cast<PsiPrivacyManager*>(account()->privacyManager());
	bool blocked = privacyManager->isContactBlocked(jid());
	blockContactConfirmationHelper(!blocked);
#endif
}

void PsiContact::blockContactConfirmationHelper(bool block)
{
	if (!account())
		return;

#ifdef YAPSI
	YaPrivacyManager* privacyManager = dynamic_cast<YaPrivacyManager*>(account()->privacyManager());
	Q_ASSERT(privacyManager);

	privacyManager->setContactBlocked(jid(), block);
#else
	PsiPrivacyManager* privacyManager = dynamic_cast<PsiPrivacyManager*>(account()->privacyManager());
	privacyManager->setContactBlocked(jid(), block);
#endif
}

void PsiContact::blockContactConfirmation(const QString& id, bool confirmed)
{
	Q_ASSERT(id == "blockContact");
	Q_UNUSED(id);
	if (confirmed) {
		blockContactConfirmationHelper(true);
	}
}

/*!
 * The only way to get dual authorization with XMPP1.0-compliant servers
 * is to request auth first and make a contact accept it, and request it
 * on its own.
 */
void PsiContact::rerequestAuthorizationFrom()
{
	if (account())
		account()->dj_authReq(jid());
}

void PsiContact::removeAuthorizationFrom()
{
	qWarning("PsiContact::removeAuthorizationFrom()");
}

void PsiContact::remove()
{
	if (account())
		account()->actionRemove(jid());
}

void PsiContact::clearCustomPicture()
{
	if (account())
		account()->avatarFactory()->removeManualAvatar(jid());
}

void PsiContact::userInfo()
{
	if (account())
		account()->actionInfo(jid());
}

void PsiContact::history()
{
#ifdef YAPSI
	if (account())
		Ya::showHistory(account(), jid());
#else
	if (account())
		account()->actionHistory(jid());
#endif
}

#ifdef YAPSI
bool PsiContact::isYaInformer() const
{
	return d->u_.groups().contains(Ya::INFORMERS_GROUP_NAME);
}

bool PsiContact::isYaJid()
{
	return Ya::isYaJid(jid().full());
}

bool PsiContact::isYandexTeamJid()
{
	return Ya::isYandexTeamJid(jid().full());
}

YaProfile PsiContact::getYaProfile() const
{
	return YaProfile(account(), jid());
}

void PsiContact::yaProfile()
{
	if (!account() || !(isYaJid() || isYandexTeamJid()))
		return;
	getYaProfile().browse();
}

void PsiContact::yaPhotos()
{
	if (!account() || !isYaJid())
		return;
	getYaProfile().browsePhotos();
}

void PsiContact::yaEmail()
{
	// FIXME: use vcard.email()?
	DesktopUtil::openEMail(jid().bare());
}
#endif

void PsiContact::addRemoveAuthBlockAvailable(bool* addButton, bool* deleteButton, bool* authButton, bool* blockButton) const
{
	Q_ASSERT(addButton && deleteButton && authButton && blockButton);
	*addButton    = false;
	*deleteButton = false;
	*authButton   = false;
	*blockButton  = false;

	*deleteButton = isRemovable();

	if (account() && account()->isAvailable() && !userListItem().isSelf()) {
		UserListItem* u = account()->findFirstRelevant(jid());

		*blockButton = isEditable();

		if (!u || !u->inList()) {
			*addButton   = isEditable();
		}
		else {
			if (!authorizesToSeeStatus()) {
				*authButton = isEditable();
			}
		}

// FIXME
#ifdef YAPSI
		YaPrivacyManager* privacyManager = dynamic_cast<YaPrivacyManager*>(account()->privacyManager());
		Q_ASSERT(privacyManager);
		*blockButton = *blockButton && privacyManager->isAvailable();
#else
		PsiPrivacyManager* privacyManager = dynamic_cast<PsiPrivacyManager*>(account()->privacyManager());
		if(privacyManager)
			*blockButton = *blockButton && privacyManager->isAvailable();
#endif
	}
}

bool PsiContact::addAvailable() const
{
	bool addButton, deleteButton, authButton, blockButton;
	addRemoveAuthBlockAvailable(&addButton, &deleteButton, &authButton, &blockButton);
	return addButton;
}

bool PsiContact::removeAvailable() const
{
	bool addButton, deleteButton, authButton, blockButton;
	addRemoveAuthBlockAvailable(&addButton, &deleteButton, &authButton, &blockButton);
	return deleteButton;
}

bool PsiContact::authAvailable() const
{
	bool addButton, deleteButton, authButton, blockButton;
	addRemoveAuthBlockAvailable(&addButton, &deleteButton, &authButton, &blockButton);
	return authButton;
}

bool PsiContact::blockAvailable() const
{
	bool addButton, deleteButton, authButton, blockButton;
	addRemoveAuthBlockAvailable(&addButton, &deleteButton, &authButton, &blockButton);
	return blockButton;
}

#ifdef YAPSI
bool PsiContact::historyAvailable() const
{
	return Ya::historyAvailable(account(), jid());
}
#endif

void PsiContact::avatarChanged(const Jid& j)
{
	if (!j.compare(jid(), false))
		return;
	emit updated();
}

void PsiContact::rereadVCard()
{
	vcardChanged(jid());
}

void PsiContact::vcardChanged(const Jid& j)
{
	if (!j.compare(jid(), false))
		return;

#ifdef YAPSI
	d->gender_ = XMPP::VCard::UnknownGender;
	const VCard vcard = VCardFactory::instance()->vcard(jid());
	if (vcard) {
		d->gender_ = vcard->gender();
	}
	d->genderCached_ = true;
#endif
	emit updated();
}

bool PsiContact::compare(const ContactListItem* other) const
{
	const ContactListGroup* group = dynamic_cast<const ContactListGroup*>(other);
	if (group) {
		return !group->compare(this);
	}

	const PsiContact* contact = dynamic_cast<const PsiContact*>(other);
	if (contact) {
		int rank = rankStatus(d->oldStatus_.type()) - rankStatus(contact->d->oldStatus_.type());
		if (rank == 0)
			rank = QString::localeAwareCompare(comparisonName().toLower(), contact->comparisonName().toLower());
		return rank < 0;
	}

	return ContactListItem::compare(other);
}

// FIXME
#ifdef YAPSI
static YaPrivacyManager* privacyManager(PsiAccount* account)
{
	return dynamic_cast<YaPrivacyManager*>(account->privacyManager());
}
#endif

bool PsiContact::isBlocked() const
{
// FIXME
#ifdef YAPSI
	return account() && privacyManager(account()) &&
	       privacyManager(account())->isContactBlocked(jid());
#else
	if(account()) {
		PsiPrivacyManager* privacyManager = dynamic_cast<PsiPrivacyManager*>(account()->privacyManager());
		return privacyManager->isContactBlocked(jid());
	}
	return false;
#endif
}

bool PsiContact::isSelf() const
{
	return false;
}

bool PsiContact::isAgent() const
{
	return userListItem().isTransport();
}

bool PsiContact::isConference() const
{
	return userListItem().isConference();
}

bool PsiContact::inList() const
{
	return userListItem().inList();
}

bool PsiContact::isValid() const
{
	return d->isValid_;
}

bool PsiContact::isPrivate() const
{
	return userListItem().isPrivate();
}

bool PsiContact::noGroups() const
{
	return userListItem().groups().isEmpty();
}

/*!
 * Returns true if contact could see our status.
 */
bool PsiContact::authorized() const
{
	return userListItem().subscription().type() == Subscription::Both ||
	       userListItem().subscription().type() == Subscription::From;
}

/*!
 * Returns true if we could see contact's status.
 */
bool PsiContact::authorizesToSeeStatus() const
{
	return userListItem().subscription().type() == Subscription::Both ||
	       userListItem().subscription().type() == Subscription::To;
}

bool PsiContact::askingForAuth() const
{
	return userListItem().ask() == "subscribe";
}

bool PsiContact::isOnline() const
{
	if (!inList() ||
	    isPrivate())
	{
		return true;
	}

	return d->status_.type()    != XMPP::Status::Offline ||
	       d->oldStatus_.type() != XMPP::Status::Offline
#ifdef YAPSI
	       || d->showOnlineTemporarily_
	       || d->reconnecting_;
#endif
	;
}

bool PsiContact::isHidden() const
{
	return userListItem().isHidden();
}

#ifdef YAPSI
void PsiContact::showOnlineTemporarily()
{
	d->showOnlineTemporarily_ = true;
	d->statusTimer_->start();
	emit updated();
}

void PsiContact::setReconnectingState(bool reconnecting)
{
	d->reconnecting_ = reconnecting;
	emit updated();
}
#endif

void PsiContact::setEditing(bool editing)
{
	if (this->editing() != editing) {
		ContactListItem::setEditing(editing);
		emit updated();
	}
}

#ifdef YAPSI
bool PsiContact::moodNotificationsEnabled() const
{
	return !account()->psi()->yaToasterCentral()->moodNotificationsDisabled(jid());
}

void PsiContact::setMoodNotificationsEnabled(bool enabled)
{
	account()->psi()->yaToasterCentral()->setMoodNotificationsDisabled(jid(), !enabled);
}
#endif

#include "psicontact.moc"
