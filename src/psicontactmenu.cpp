/*
 * psicontactmenu.cpp - a PsiContact context menu
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

#include "psicontactmenu.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QFileDialog>

#include "iconaction.h"
#include "iconset.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psioptions.h"
#include "resourcemenu.h"
#include "contactlistmodel.h"
#include "shortcutmanager.h"
#include "psicon.h"
#include "avatars.h"
#include "userlist.h"
#include "statusdlg.h"
#include "xmpp_tasks.h"
#include "avcall/avcall.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif

#ifdef YAPSI
#include "yaprofile.h"
#else
#include "psiprivacymanager.h"
#endif

#include "groupchatdlg.h"

//----------------------------------------------------------------------------
// GroupMenu
//----------------------------------------------------------------------------

class GroupMenu : public QMenu
{
	Q_OBJECT
public:
	GroupMenu(QWidget* parent)
		: QMenu(parent)
	{
	}

	void updateMenu(PsiContact* contact)
	{
		if (isVisible())
			return;
		contact_ = contact;
		Q_ASSERT(contact_);
		clear();

		addGroup(tr("&None"), "", contact->userListItem().groups().isEmpty());
		addSeparator();

		int n = 0;
		QStringList groupList = contact->account()->groupList();
		groupList.removeAll(PsiContact::hiddenGroupName());
		foreach(QString groupName, groupList) {
			QString displayName = groupName;
			if (displayName.isEmpty())
				displayName = PsiContact::generalGroupName();

			QString accelerator;
			if (n++ < 9)
				accelerator = "&";
			QString text = QString("%1%2. %3").arg(accelerator).arg(n).arg(displayName);
			addGroup(text, groupName, contact->userListItem().groups().contains(groupName));
		}

		addSeparator();
		addGroup(tr("&Hidden"), PsiContact::hiddenGroupName(), contact->isHidden());
		addSeparator();

		QAction* createNewGroupAction = new QAction(tr("&Create New..."), this);
		connect(createNewGroupAction, SIGNAL(triggered()), SLOT(createNewGroup()));
		addAction(createNewGroupAction);
	}

signals:
	void groupActivated(QString groupName);

private:
	QPointer<PsiContact> contact_;
	QAction* createNewGroupAction_;

	/**
	 * \param text will be shown on screen, and \param groupName is the
	 * actual group name. Specify true as \param current when group is
	 * currently selected for a contact.
	 */
	void addGroup(QString text, QString groupName, bool selected)
	{
		QAction* action = new QAction(text, this);
		addAction(action);
		action->setCheckable(true);
		action->setChecked(selected);
		action->setProperty("groupName", QVariant(groupName));
		connect(action, SIGNAL(triggered()), SLOT(actionActivated()));
	}

private slots:
	void actionActivated()
	{
		QAction* action = static_cast<QAction*>(sender());
		emit groupActivated(action->property("groupName").toString());
	}

	void createNewGroup()
	{
		while (contact_) {
			bool ok = false;
			QString newgroup = QInputDialog::getText(0, tr("Create New Group"),
			                   tr("Enter the new group name:"),
			                   QLineEdit::Normal,
			                   QString::null,
			                   &ok, 0);
			if (!ok)
				break;
			if (newgroup.isEmpty())
				continue;

			if (!contact_->userListItem().groups().contains(newgroup)) {
				emit groupActivated(newgroup);
				break;
			}
		}
	}
};

//----------------------------------------------------------------------------
// InviteToGroupChatMenu
//----------------------------------------------------------------------------

class InviteToGroupChatMenu : public QMenu
{
	Q_OBJECT
public:
	InviteToGroupChatMenu(QWidget* parent)
		: QMenu(parent)
	{
	}

	void updateMenu(PsiContact* contact)
	{
		if (isVisible())
			return;
		Q_ASSERT(contact);
		controller_ = contact->account()->psi();
		Q_ASSERT(controller_);
		clear();

		foreach(PsiAccount* acc, controller_->contactList()->accounts()) {
			foreach(QString groupChat, acc->groupchats()) {
				QAction* action = new QAction(groupChat, this);
				addAction(action);

				action->setProperty("groupChat", QVariant(groupChat));
				action->setProperty("account", QVariant(acc->id()));
				connect(action, SIGNAL(triggered()), SLOT(actionActivated()));
			}
		}
	}

signals:
	void inviteToGroupchat(PsiAccount* account, QString groupChat);

private slots:
	void actionActivated()
	{
		QAction* action = static_cast<QAction*>(sender());
		emit inviteToGroupchat(controller_->contactList()->getAccount(action->property("account").toString()),
		                       action->property("groupChat").toString());
	}

private:
	PsiCon* controller_;
};

//----------------------------------------------------------------------------
// PsiContactMenu::Private
//----------------------------------------------------------------------------

class PsiContactMenu::Private : public QObject
{
	Q_OBJECT

	QPointer<PsiContact> contact_;
	PsiContactMenu* menu_;

public:
	QAction* renameAction_;
	QAction* removeAction_;

#ifdef YAPSI
	QAction* openChatAction_;
	QAction* openHistoryAction_;
	QAction* yaProfileAction_;
	QAction* yaPhotosAction_;
	QAction* yaEmailAction_;
	QAction* addAction_;
	QAction* authAction_;
	QAction* blockAction_;
	QAction* disableMoodNotificationsAction_;
#else
	QAction* addAuthAction_;
	QAction* transportLogonAction_;
	QAction* transportLogoffAction_;
	QAction* receiveIncomingEventAction_;
	QMenu* msgMenu_;
	QAction* sendMessageAction_;
	QMenu* sendMessageToMenu_;
	QAction* openChatAction_;
	QMenu* openChatToMenu_;
	QAction* openWhiteboardAction_;
	QMenu* openWhiteboardToMenu_;
	ResourceMenu* executeCommandMenu_;
	ResourceMenu* activeChatsMenu_;
	QAction* voiceCallAction_;
	QAction* sendFileAction_;
	QAction* customStatusAction_;
	InviteToGroupChatMenu* inviteToGroupchatMenu_;
	QMenu* mngMenu_;
	GroupMenu* groupMenu_;
	QMenu* authMenu_;
	QAction* authResendAction_;
	QAction* authRerequestAction_;
	QAction* authRemoveAction_;
	QMenu* pictureMenu_;
	QAction* pictureAssignAction_;
	QAction* pictureClearAction_;
	QAction* gpgAssignKeyAction_;
	QAction* gpgUnassignKeyAction_;
	QAction* vcardAction_;
	QAction* historyAction_;
	QAction* mucHideAction_;
	QAction* mucShowAction_;
	QAction* mucLeaveAction_;
	QAction* blockAction_;

#endif

public:
	Private(PsiContactMenu* menu, PsiContact* _contact)
		: QObject(0)
		, contact_(_contact)
		, menu_(menu)
	{
		connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
		connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));

		connect(contact_, SIGNAL(updated()), SLOT(updateActions()));

		renameAction_ = new QAction(tr("Re&name"), this);
		renameAction_->setShortcuts(menu->shortcuts("contactlist.rename"));
		connect(renameAction_, SIGNAL(triggered()), this, SLOT(rename()));

		removeAction_ = new QAction(tr("&Remove"), this);
		removeAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
		connect(removeAction_, SIGNAL(triggered()), SLOT(removeContact()));

#ifdef YAPSI
		openChatAction_ = new QAction(tr("&Chat"), this);
		connect(openChatAction_, SIGNAL(triggered()), contact_, SLOT(openChat()));

		openHistoryAction_ = new QAction(tr("&History"), this);
		connect(openHistoryAction_, SIGNAL(triggered()), contact_, SLOT(history()));

		yaProfileAction_ = new QAction(tr("Pro&file"), this);
		connect(yaProfileAction_, SIGNAL(triggered()), contact_, SLOT(yaProfile()));

		yaPhotosAction_ = new QAction(tr("&Photos"), this);
		connect(yaPhotosAction_, SIGNAL(triggered()), contact_, SLOT(yaPhotos()));

		yaEmailAction_ = new QAction(tr("Send &E-mail"), this);
		connect(yaEmailAction_, SIGNAL(triggered()), contact_, SLOT(yaEmail()));

		addAction_ = new QAction(tr("&Add"), this);
		connect(addAction_, SIGNAL(triggered()), SLOT(addContact()));

		authAction_ = new QAction(tr("A&uth"), this);
		connect(authAction_, SIGNAL(triggered()), contact_, SLOT(rerequestAuthorizationFrom()));

		blockAction_ = new QAction(tr("&Block"), this);
		connect(blockAction_, SIGNAL(triggered()), contact_, SLOT(toggleBlockedState()));

		disableMoodNotificationsAction_ = new QAction(tr("Disable mood notifications"), this);
		disableMoodNotificationsAction_->setCheckable(true);
		connect(disableMoodNotificationsAction_, SIGNAL(triggered()), SLOT(disableMoodNotificationsTriggered()));

		menu->addAction(openChatAction_);
		menu->addAction(yaEmailAction_);
		menu->addAction(openHistoryAction_);
		menu->addAction(yaProfileAction_);
		menu->addAction(yaPhotosAction_);
		menu->addSeparator();
		menu->addAction(renameAction_);
		menu->addAction(removeAction_);
		menu->addAction(addAction_);
		menu->addAction(authAction_);
		menu->addAction(blockAction_);
		menu_->addSeparator();
		menu_->addAction(disableMoodNotificationsAction_);
		updateActions();
#else
		addAuthAction_ = new IconAction(tr("Add/Authorize to Contact List"), this, "psi/addContact");
		connect(addAuthAction_, SIGNAL(triggered()), SLOT(addAuth()));

		transportLogonAction_ = new IconAction(tr("&Log On"), this, "");
		connect(transportLogonAction_, SIGNAL(triggered()), SLOT(transportLogon()));
		transportLogonAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.login-transport"));

		transportLogoffAction_ = new IconAction(tr("Log &Off"), this, "");
		connect(transportLogoffAction_, SIGNAL(triggered()), SLOT(transportLogoff()));

		receiveIncomingEventAction_ = new IconAction(tr("&Receive Incoming Event"), this, "");
		connect(receiveIncomingEventAction_, SIGNAL(triggered()), SLOT(receiveIncomingEvent()));
		receiveIncomingEventAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.event"));

		sendMessageAction_ = new IconAction(tr("Send &Message"), this, "psi/sendMessage");
		connect(sendMessageAction_, SIGNAL(triggered()), SLOT(sendMessage()));
		sendMessageAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.message"));

		openChatAction_ = new IconAction(tr("Open &Chat Window"), this, "psi/start-chat");
		connect(openChatAction_, SIGNAL(triggered()), SLOT(openChat()));
		openChatAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.chat"));

		openWhiteboardAction_ = new IconAction(tr("Open a &Whiteboard"), this, "psi/whiteboard");
		connect(openWhiteboardAction_, SIGNAL(triggered()), SLOT(openWhiteboard()));

		voiceCallAction_ = new IconAction(tr("Voice Call"), this, "psi/avcall");
		connect(voiceCallAction_, SIGNAL(triggered()), SLOT(voiceCall()));

		sendFileAction_ = new IconAction(tr("Send &File"), this, "psi/upload");
		connect(sendFileAction_, SIGNAL(triggered()), SLOT(sendFile()));

		authResendAction_ = new IconAction(tr("Re&send Authorization To"), this, "");
		connect(authResendAction_, SIGNAL(triggered()), SLOT(authResend()));

		authRerequestAction_ = new IconAction(tr("Re&request Authorization From"), this, "");
		connect(authRerequestAction_, SIGNAL(triggered()), SLOT(authRerequest()));

		authRemoveAction_ = new IconAction(tr("Re&move Authorization From"), this, "");
		connect(authRemoveAction_, SIGNAL(triggered()), SLOT(authRemove()));

		customStatusAction_ = new IconAction(tr("Sen&d Status"), this, "psi/action_direct_presence");
		connect(customStatusAction_, SIGNAL(triggered()), SLOT(customStatus()));

		pictureAssignAction_ = new IconAction(tr("&Assign Custom Picture"), this, "");
		connect(pictureAssignAction_, SIGNAL(triggered()), SLOT(pictureAssign()));
		pictureAssignAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.assign-custom-avatar"));

		pictureClearAction_ = new IconAction(tr("&Clear Custom Picture"), this, "");
		connect(pictureClearAction_, SIGNAL(triggered()), SLOT(pictureClear()));
		pictureClearAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.clear-custom-avatar"));

		gpgAssignKeyAction_ = new IconAction(tr("Assign Open&PGP Key"), this, "psi/gpg-yes");
		connect(gpgAssignKeyAction_, SIGNAL(triggered()), SLOT(gpgAssignKey()));

		gpgUnassignKeyAction_ = new IconAction(tr("Unassign Open&PGP Key"), this, "psi/gpg-no");
		connect(gpgUnassignKeyAction_, SIGNAL(triggered()), SLOT(gpgUnassignKey()));

		vcardAction_ = new IconAction(tr("User &Info"), this, "psi/vCard");
		connect(vcardAction_, SIGNAL(triggered()), SLOT(vcard()));
		vcardAction_->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));

		historyAction_ = new IconAction(tr("&History"), this, "psi/history");
		connect(historyAction_, SIGNAL(triggered()), SLOT(history()));
		historyAction_->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));

		inviteToGroupchatMenu_ = new InviteToGroupChatMenu(menu_);
		inviteToGroupchatMenu_->setTitle(tr("In&vite To"));
		inviteToGroupchatMenu_->setIcon(IconsetFactory::icon("psi/groupChat").icon());
		connect(inviteToGroupchatMenu_, SIGNAL(inviteToGroupchat(PsiAccount*, QString)), SLOT(inviteToGroupchat(PsiAccount*, QString)));

		groupMenu_ = new GroupMenu(menu_);
		groupMenu_->setTitle(tr("&Group"));
		connect(groupMenu_, SIGNAL(groupActivated(QString)), SLOT(setContactGroup(QString)));

		sendMessageToMenu_ = new ResourceMenu(tr("Send Message T&o"), contact_, menu_);
		connect(sendMessageToMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(sendMessageTo(PsiContact*, const XMPP::Jid&)));

		openChatToMenu_ = new ResourceMenu(tr("Open Chat &To"), contact_, menu_);
		connect(openChatToMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(openChatTo(PsiContact*, const XMPP::Jid&)));

		openWhiteboardToMenu_ = new ResourceMenu(tr("Open a White&board To"), contact_, menu_);
		connect(openWhiteboardToMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(openWhiteboardTo(PsiContact*, const XMPP::Jid&)));

		executeCommandMenu_ = new ResourceMenu(tr("E&xecute Command"), contact_, menu_);
		connect(executeCommandMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(executeCommand(PsiContact*, const XMPP::Jid&)));

		activeChatsMenu_ = new ResourceMenu(tr("&Active Chats"), contact_, menu_);
		activeChatsMenu_->setActiveChatsMode(true);
		connect(activeChatsMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(openActiveChat(PsiContact*, const XMPP::Jid&)));

		mucHideAction_ = new IconAction(tr("Hide"), this, "psi/action_muc_hide");
		connect(mucHideAction_, SIGNAL(triggered()), SLOT(mucHide()));
		mucHideAction_->setShortcuts(ShortcutManager::instance()->shortcuts("common.hide"));

		mucShowAction_ = new IconAction(tr("Show"), this, "psi/action_muc_show");
		connect(mucShowAction_, SIGNAL(triggered()), SLOT(mucShow()));
		mucShowAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.chat"));

		mucLeaveAction_ = new IconAction(tr("Leave"), this, "psi/action_muc_leave");
		connect(mucLeaveAction_, SIGNAL(triggered()), SLOT(mucLeave()));
		mucLeaveAction_->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));

		blockAction_ = new IconAction(tr("Block"), "psi/stop", tr("Block"), 0, this, 0, true);
		connect(blockAction_, SIGNAL(triggered(bool)), SLOT(block(bool)));

		if (!contact_->isConference()) {
			menu_->addAction(addAuthAction_);
			menu_->addAction(transportLogonAction_);
			menu_->addAction(transportLogoffAction_);
			menu_->addSeparator();
			menu_->addAction(receiveIncomingEventAction_);
			menu_->addSeparator();
			msgMenu_ = menu_->addMenu(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &Message"));
			msgMenu_->addAction(sendMessageAction_);
			msgMenu_->addMenu(sendMessageToMenu_);
			msgMenu_->addAction(openChatAction_);
			msgMenu_->addMenu(openChatToMenu_);
			menu_->addAction(openWhiteboardAction_);
			menu_->addMenu(openWhiteboardToMenu_);
			menu_->addMenu(executeCommandMenu_);
			menu_->addMenu(activeChatsMenu_);
			if(AvCallManager::isSupported()) {
				menu_->addAction(voiceCallAction_);
			}
			menu_->addSeparator();
			menu_->addAction(sendFileAction_);
			menu_->addMenu(inviteToGroupchatMenu_);
			menu_->addSeparator();
			mngMenu_ = menu_->addMenu(IconsetFactory::icon("psi/manageContact").icon(), tr("Manage &Contact"));
			mngMenu_->addAction(renameAction_);
			mngMenu_->addMenu(groupMenu_);
			mngMenu_->addAction(blockAction_);
			authMenu_ = mngMenu_->addMenu(tr("&Authorization"));
			authMenu_->addAction(authResendAction_);
			authMenu_->addAction(authRerequestAction_);
			authMenu_->addAction(authRemoveAction_);
			mngMenu_->addAction(removeAction_);
			menu_->addAction(customStatusAction_);
			menu_->addSeparator();
			pictureMenu_ = mngMenu_->addMenu(tr("&Picture"));
			pictureMenu_->addAction(pictureAssignAction_);
			pictureMenu_->addAction(pictureClearAction_);
			mngMenu_->addAction(gpgAssignKeyAction_);
			mngMenu_->addAction(gpgUnassignKeyAction_);
			menu_->addAction(vcardAction_);
			menu_->addAction(historyAction_);
			updateActions();
		}
		else {
			menu_->addAction(mucHideAction_);
			menu_->addAction(mucShowAction_);
			menu_->addAction(mucLeaveAction_);
			menu_->addSeparator();
			menu_->addAction(customStatusAction_);
			//menu_->addAction(blockAction_);
		}
#endif
	}

private slots:
	void optionChanged(const QString& option)
	{
#ifdef YAPSI
		if (option == "options.ya.popups.moods.enable") {
			updateActions();
		}
#else
		Q_UNUSED(option);
#endif
	}

	void updateActions()
	{
		if (!contact_)
			return;

		if(contact_->isConference()) {
			updateBlockActionState();
			return;
		}

#ifdef YAPSI
		YaProfile* profile = YaProfile::create(contact_->account(), contact_->jid());
		openHistoryAction_->setEnabled(contact_->historyAvailable());
		delete profile;

		yaProfileAction_->setEnabled(contact_->isYaJid());
		yaPhotosAction_->setEnabled(contact_->isYaJid());
		addAction_->setVisible(contact_->addAvailable());
		authAction_->setVisible(contact_->authAvailable());
		blockAction_->setEnabled(contact_->blockAvailable());

		blockAction_->setText(contact_->isBlocked() ?
		               tr("&Unblock") : tr("&Block"));

		disableMoodNotificationsAction_->setChecked(!contact_->moodNotificationsEnabled());
		disableMoodNotificationsAction_->setEnabled(PsiOptions::instance()->getOption("options.ya.popups.moods.enable").toBool());
#else
		inviteToGroupchatMenu_->updateMenu(contact_);
		groupMenu_->updateMenu(contact_);

		addAuthAction_->setVisible(!contact_->isSelf() && !contact_->inList() && !PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool());
		addAuthAction_->setEnabled(contact_->account()->isAvailable());
		customStatusAction_->setEnabled(contact_->account()->isAvailable() && !contact_->isPrivate());
		receiveIncomingEventAction_->setVisible(contact_->alerting());
		if (!PsiOptions::instance()->getOption("options.ui.message.enabled").toBool()) {
			sendMessageAction_->setVisible(false);
			sendMessageToMenu_->menuAction()->setVisible(false);
		}
		sendMessageAction_->setEnabled(contact_->account()->isAvailable());
		sendMessageToMenu_->setEnabled(!sendMessageToMenu_->isEmpty());
		openChatToMenu_->setEnabled(!openChatToMenu_->isEmpty());
		openWhiteboardToMenu_->setEnabled(!openWhiteboardToMenu_->isEmpty());
#ifndef WHITEBOARDING
		openWhiteboardAction_->setVisible(false);
		openWhiteboardToMenu_->menuAction()->setVisible(false);
#endif
		if(contact_->account()->isAvailable()
			&& executeCommandMenu_->isEmpty()
			&& contact_->status().type() == Status::Offline )
			executeCommandMenu_->addResource(XMPP::Status::Offline, "");
		executeCommandMenu_->setEnabled(!executeCommandMenu_->isEmpty());
		activeChatsMenu_->setEnabled(!activeChatsMenu_->isEmpty());
		activeChatsMenu_->menuAction()->setVisible(PsiOptions::instance()->getOption("options.ui.menu.contact.active-chats").toBool());
		voiceCallAction_->setVisible(contact_->account()->avCallManager() && !contact_->isAgent());
		voiceCallAction_->setEnabled(contact_->account()->isAvailable());
		sendFileAction_->setVisible(!contact_->isAgent());
		sendFileAction_->setEnabled(contact_->account()->isAvailable());
		inviteToGroupchatMenu_->setEnabled(!inviteToGroupchatMenu_->isEmpty() && contact_->account()->isAvailable());
		renameAction_->setVisible(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool());
		renameAction_->setEnabled(contact_->isEditable());
		if (contact_->isAgent()) {
			groupMenu_->menuAction()->setVisible(false);
		}
		groupMenu_->setEnabled(contact_->isEditable() && contact_->isDragEnabled());
		transportLogonAction_->setVisible(contact_->isAgent());
		transportLogonAction_->setEnabled(contact_->account()->isAvailable() && contact_->status().type() == XMPP::Status::Offline);
		transportLogoffAction_->setVisible(contact_->isAgent());
		transportLogoffAction_->setEnabled(contact_->account()->isAvailable() && contact_->status().type() != XMPP::Status::Offline);
		if (PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool() || !contact_->inList()) {
			authMenu_->menuAction()->setVisible(false);
		}
		authMenu_->setEnabled(contact_->account()->isAvailable());
		updateBlockActionState();
		removeAction_->setVisible(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()  && !contact_->isSelf());
		removeAction_->setEnabled(contact_->removeAvailable());
		if (!PsiOptions::instance()->getOption("options.ui.menu.contact.custom-picture").toBool()) {
			pictureMenu_->menuAction()->setVisible(false);
		}
#ifdef HAVE_PGPUTIL
		gpgAssignKeyAction_->setVisible(PGPUtil::instance().pgpAvailable() && PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool() && contact_->userListItem().publicKeyID().isEmpty());
		gpgUnassignKeyAction_->setVisible(PGPUtil::instance().pgpAvailable() && PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool() && !contact_->userListItem().publicKeyID().isEmpty());
#endif // HAVE_PGPUTIL
#endif // YAPSI
	}

#ifdef YAPSI
	void disableMoodNotificationsTriggered()
	{
		if (contact_) {
			contact_->setMoodNotificationsEnabled(!disableMoodNotificationsAction_->isChecked());
		}
	}
#endif


	void mucHide()
	{
		GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
		if (gc && (gc->isTabbed() || !gc->isHidden()))
			gc->hideTab();
	}

	void mucShow()
	{
		GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
		if (gc) {
			gc->ensureTabbedCorrectly();
			gc->bringToFront();
		}
	}

	void mucLeave()
	{
		GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
		if (gc)
			gc->close();
	}

	void rename()
	{
		if (!contact_)
			return;

		menu_->model()->renameSelectedItem();
	}

	void addContact()
	{
		emit menu_->addSelection();
	}

	void removeContact()
	{
		emit menu_->removeSelection();
	}

#ifndef YAPSI
	void inviteToGroupchat(PsiAccount* account, QString groupchat)
	{
		if (!contact_)
			return;
		account->actionInvite(contact_->jid(), groupchat);
		QMessageBox::information(0, tr("Invitation"),
		tr("Sent groupchat invitation to <b>%1</b>.").arg(contact_->name()));
	}

	void setContactGroup(QString group)
	{
		if (!contact_)
			return;
		contact_->setGroups(QStringList() << group);
	}

	void block(bool )
	{
		if (!contact_)
			return;

		contact_->toggleBlockedStateConfirmation();
	}

	void addAuth()
	{
		if (!contact_)
			return;
		contact_->account()->actionAdd(contact_->jid());
		contact_->account()->actionAuth(contact_->jid());
		QMessageBox::information(0, tr("Add"),
		tr("Added/Authorized <b>%1</b> to the contact list.").arg(contact_->name()));
	}

	void receiveIncomingEvent()
	{
		if (!contact_)
			return;
		contact_->account()->actionRecvEvent(contact_->jid());
	}

	void sendMessage()
	{
		if (!contact_)
			return;
		contact_->account()->actionSendMessage(contact_->jid());
	}

	void openChat()
	{
		if (!contact_)
			return;
		contact_->account()->actionOpenChat(contact_->jid());
	}

	void openWhiteboard()
	{
		if (!contact_)
			return;
#ifdef WHITEBOARDING
		contact_->account()->actionOpenWhiteboard(contact_->jid());
#endif
	}

	void voiceCall()
	{
		if (!contact_)
			return;
		contact_->account()->actionVoice(contact_->jid());
	}

	void sendFile()
	{
		if (!contact_)
			return;
		contact_->account()->actionSendFile(contact_->jid());
	}

	void transportLogon()
	{
		if (!contact_)
			return;
		contact_->account()->actionAgentSetStatus(contact_->jid(), contact_->account()->status());
	}

	void transportLogoff()
	{
		if (!contact_)
			return;
		contact_->account()->actionAgentSetStatus(contact_->jid(), XMPP::Status::Offline);
	}

	void authResend()
	{
		if (!contact_)
			return;
		contact_->account()->actionAuth(contact_->jid());
		QMessageBox::information(0, tr("Authorize"),
		tr("Sent authorization to <b>%1</b>.").arg(contact_->name()));
	}

	void authRerequest()
	{
		if (!contact_)
			return;
		contact_->account()->actionAuthRequest(contact_->jid());
		QMessageBox::information(0, tr("Authorize"),
		tr("Rerequested authorization from <b>%1</b>.").arg(contact_->name()));
	}

	void authRemove()
	{
		if (!contact_)
			return;

		int n = QMessageBox::information(0, tr("Remove"),
		tr("Are you sure you want to remove authorization from <b>%1</b>?").arg(contact_->name()),
		tr("&Yes"), tr("&No"));

		if(n == 0)
			contact_->account()->actionAuthRemove(contact_->jid());
	}

	void customStatus()
	{
		if (!contact_)
			return;

		StatusSetDlg *w = new StatusSetDlg(contact_->account()->psi(), makeLastStatus(contact_->account()->status().type()), lastPriorityNotEmpty());
		w->setJid(contact_->jid());
		connect(w, SIGNAL(setJid(const Jid &, const Status &)), SLOT(setStatusFromDialog(const Jid &, const Status &)));
		w->show();
	}

	void setStatusFromDialog(const Jid &j, const Status &s)
	{
		JT_Presence *p = new JT_Presence(contact_->account()->client()->rootTask());
		p->pres(j,s);
		p->go(true);
	}

	void pictureAssign()
	{
		if (!contact_)
			return;
		QString file = QFileDialog::getOpenFileName(0, tr("Choose an Image"), "", tr("All files (*.png *.jpg *.gif)"));
		if (!file.isNull()) {
			contact_->account()->avatarFactory()->importManualAvatar(contact_->jid(),file);
		}
	}

	void pictureClear()
	{
		if (!contact_)
			return;
		contact_->account()->avatarFactory()->removeManualAvatar(contact_->jid());
	}

	void gpgAssignKey()
	{
		if (!contact_)
			return;
		contact_->account()->actionAssignKey(contact_->jid());
	}

	void gpgUnassignKey()
	{
		if (!contact_)
			return;
		contact_->account()->actionUnassignKey(contact_->jid());
	}

	void vcard()
	{
		if (!contact_)
			return;
		contact_->account()->actionInfo(contact_->jid());
	}

	void history()
	{
		if (!contact_)
			return;
		contact_->account()->actionHistory(contact_->jid());
	}

	void sendMessageTo(PsiContact*, const XMPP::Jid& jid)
	{
		if (!contact_)
			return;

		contact_->account()->actionSendMessage(jid);
	}

	void openChatTo(PsiContact*, const XMPP::Jid& jid)
	{
		if (!contact_)
			return;

		contact_->account()->actionOpenChatSpecific(jid);
	}

	void openWhiteboardTo(PsiContact*, const XMPP::Jid& jid)
	{
		if (!contact_)
			return;

#ifdef WHITEBOARDING
		contact_->account()->actionOpenWhiteboardSpecific(jid);
#else
		Q_UNUSED(jid);
#endif
	}

	void executeCommand(PsiContact*, const XMPP::Jid& jid)
	{
		if (!contact_)
			return;

		contact_->account()->actionExecuteCommandSpecific(jid);
	}

	void openActiveChat(PsiContact*, const XMPP::Jid& jid)
	{
		if (!contact_)
			return;

		contact_->account()->actionOpenChatSpecific(jid);
	}
#endif
private:
	void updateBlockActionState()
	{
		if(!contact_)
			return;
		blockAction_->setVisible(!(contact_->isPrivate()/* || contact_->isAgent()*/ || contact_->isSelf()));
		blockAction_->setEnabled(contact_->account()->isAvailable() && dynamic_cast<PsiPrivacyManager*>(contact_->account()->privacyManager())->isAvailable());
		blockAction_->setChecked(contact_->isBlocked());
		blockAction_->setText(blockAction_->isChecked() ? tr("Unblock") : tr("Block"));
	}
};

PsiContactMenu::PsiContactMenu(PsiContact* contact, ContactListModel* model)
	: ContactListItemMenu(contact, model)
{
	d = new Private(this, contact);
}

PsiContactMenu::~PsiContactMenu()
{
	delete d;
}

QList<QAction*> PsiContactMenu::availableActions() const
{
	QList<QAction*> result;
	foreach(QAction* a, ContactListItemMenu::availableActions()) {
		// if (a != d->removeAction_)
			result << a;
	}
	return result;
}

#include "psicontactmenu.moc"
