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

#include "psicontactmenu_p.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QFileDialog>
#include <QWidgetAction>
#include <QApplication>
#include <QClipboard>

#include "iconaction.h"
#include "iconset.h"
#include "psicontact.h"
#include "psioptions.h"
#include "contactlistmodel.h"
#include "shortcutmanager.h"
#include "psicon.h"
#include "avatars.h"
#include "userlist.h"
#include "xmpp_tasks.h"
#include "avcall/avcall.h"
#include "pluginmanager.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#include "invitetogroupchatmenu.h"

#include "psiprivacymanager.h"

#include "groupchatdlg.h"

//----------------------------------------------------------------------------
// GroupMenu
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// PsiContactMenu::Private
//----------------------------------------------------------------------------

PsiContactMenu::Private::Private(PsiContactMenu* menu, PsiContact* _contact)
	: QObject(0)
	, contact_(_contact)
	, menu_(menu)
	, authMenu_(0)
{
	Jid jid = _contact->jid();
	menu->setLabelTitle(_contact->isPrivate() ? jid.full() : jid.bare());
	connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));

	connect(contact_, SIGNAL(updated()), SLOT(updateActions()));

	renameAction_ = new QAction(tr("Re&name"), this);
	renameAction_->setShortcuts(menu->shortcuts("contactlist.rename"));
	connect(renameAction_, SIGNAL(triggered()), this, SLOT(rename()));

	removeAction_ = new QAction(tr("&Remove"), this);
	removeAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
	connect(removeAction_, SIGNAL(triggered()), SLOT(removeContact()));

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

#ifdef WHITEBOARDING
	openWhiteboardAction_ = new IconAction(tr("Open a &Whiteboard"), this, "psi/whiteboard");
	connect(openWhiteboardAction_, SIGNAL(triggered()), SLOT(openWhiteboard()));
#endif
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

#ifdef WHITEBOARDING
	openWhiteboardToMenu_ = new ResourceMenu(tr("Open a White&board To"), contact_, menu_);
	connect(openWhiteboardToMenu_, SIGNAL(resourceActivated(PsiContact*, const XMPP::Jid&)), SLOT(openWhiteboardTo(PsiContact*, const XMPP::Jid&)));
#endif
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

	visibleAction_ = new IconAction(tr("Always Visible"), "psi/eye", tr("Always Visible"), 0, this, 0, true);
	connect(visibleAction_, SIGNAL(triggered(bool)), SLOT(setAlwaysVisible(bool)));

	_copyUserJid = new IconAction(tr("Copy User JID"), "", tr("Copy User JID"), 0 , this);
	connect(_copyUserJid, SIGNAL(triggered(bool)), SLOT(copyJid()));

	_copyMucJid = new IconAction(tr("Copy Groupchat JID"), "", tr("Copy Groupchat JID"), 0 , this);
	connect(_copyMucJid, SIGNAL(triggered(bool)), SLOT(copyJid()));

	if (!contact_->isConference()) {
		menu_->addAction(addAuthAction_);
		menu_->addAction(transportLogonAction_);
		menu_->addAction(transportLogoffAction_);
		_separator1 = menu_->addSeparator();

		menu_->addAction(receiveIncomingEventAction_);
		_separator2 = menu_->addSeparator();

		msgMenu_ = menu_->addMenu(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &Message"));
		msgMenu_->addAction(sendMessageAction_);
		msgMenu_->addMenu(sendMessageToMenu_);
		msgMenu_->addAction(openChatAction_);
		msgMenu_->addMenu(openChatToMenu_);
		menu_->addAction(sendFileAction_);
		if(AvCallManager::isSupported()) {
			menu_->addAction(voiceCallAction_);
		}
		menu_->addSeparator();

		menu_->addAction(vcardAction_);
		menu_->addAction(historyAction_);
		menu_->addSeparator();

		menu_->addAction(renameAction_);
		menu_->addAction(removeAction_);
		menu_->addMenu(groupMenu_);
		menu_->addSeparator();

		menu_->addAction(blockAction_);
		menu_->addSeparator();

		const UserListItem &item = contact_->userListItem();

		switch (item.subscription().type()) {
		case Subscription::From:
			menu_->addAction(authRerequestAction_);
			authRerequestAction_->setIcon(IconsetFactory::icon("psi/events").icon());
			break;

		case Subscription::To:
			menu_->addAction(authResendAction_);
			authResendAction_->setIcon(IconsetFactory::icon("psi/events").icon());
			break;

		case Subscription::None:
			authMenu_ = menu_->addMenu(tr("&Authorization"));
			authMenu_->setIcon(IconsetFactory::icon("psi/events").icon());
			authMenu_->addAction(authResendAction_);
			authMenu_->addAction(authRerequestAction_);
			break;

		default:
			break;
		}

		pictureMenu_ = menu_->addMenu(tr("&Picture"));
		pictureMenu_->addAction(pictureAssignAction_);
		pictureMenu_->addAction(pictureClearAction_);

		menu_->addSeparator();
		menu_->addMenu(inviteToGroupchatMenu_);
		menu_->addMenu(executeCommandMenu_);
		menu_->addMenu(activeChatsMenu_);

		menu_->addSeparator();

		_advancedMenu = menu_->addMenu(tr("Advanc&ed"));
		if (!contact_->isPrivate())
			_advancedMenu->addAction(_copyUserJid);
		_advancedMenu->addAction(customStatusAction_);
		_advancedMenu->addAction(visibleAction_);
		_advancedMenu->addAction(authRemoveAction_);
		_advancedMenu->addAction(gpgAssignKeyAction_);
		_advancedMenu->addAction(gpgUnassignKeyAction_);
#ifdef WHITEBOARDING
		_advancedMenu->addAction(openWhiteboardAction_);
		_advancedMenu->addMenu(openWhiteboardToMenu_);
#endif

#ifdef PSI_PLUGINS
		PluginManager::instance()->addContactMenu(_advancedMenu, contact_->account(), contact_->jid().full());
#endif

		updateActions();
	}
	else {
		menu_->addAction(mucHideAction_);
		menu_->addAction(mucShowAction_);
		menu_->addAction(mucLeaveAction_);
		menu_->addSeparator();
		menu_->addAction(customStatusAction_);
		menu_->addAction(_copyMucJid);
	}
}

void PsiContactMenu::Private::updateActions()
{
	if (!contact_)
		return;

	if(contact_->isConference()) {
		updateBlockActionState();
		return;
	}

	inviteToGroupchatMenu_->updateMenu(contact_);
	groupMenu_->updateMenu(contact_);

	addAuthAction_->setVisible(!contact_->isSelf() && !contact_->inList() && !PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool());
	addAuthAction_->setEnabled(contact_->account()->isAvailable());
	customStatusAction_->setEnabled(contact_->account()->isAvailable() && !contact_->isPrivate());

	receiveIncomingEventAction_->setVisible(contact_->alerting());
	_separator2->setVisible(contact_->alerting());

	sendMessageToMenu_->menuAction()->setVisible(false);
	sendMessageAction_->setEnabled(contact_->account()->isAvailable());
	sendMessageToMenu_->setEnabled(!sendMessageToMenu_->isEmpty());
	openChatToMenu_->setEnabled(!openChatToMenu_->isEmpty());
#ifdef WHITEBOARDING
	openWhiteboardToMenu_->setEnabled(!openWhiteboardToMenu_->isEmpty());
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
	_separator1->setVisible(transportLogonAction_->isVisible() || addAuthAction_->isVisible() || transportLogoffAction_->isVisible());

	bool showAuth = !(PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool() || !contact_->inList());

	if (authMenu_) {
		authMenu_->menuAction()->setVisible(showAuth);
		authMenu_->setEnabled(contact_->account()->isAvailable());
	}

	authResendAction_->setVisible(showAuth);
	authResendAction_->setEnabled(contact_->account()->isAvailable());

	authRerequestAction_->setVisible(showAuth);
	authRerequestAction_->setEnabled(contact_->account()->isAvailable());

	authRemoveAction_->setVisible(showAuth);
	authRemoveAction_->setEnabled(contact_->account()->isAvailable());

	updateBlockActionState();
	visibleAction_->setChecked(contact_->isAlwaysVisible());
	removeAction_->setVisible(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()  && !contact_->isSelf());
	removeAction_->setEnabled(contact_->removeAvailable());
	if (!PsiOptions::instance()->getOption("options.ui.menu.contact.custom-picture").toBool()) {
		pictureMenu_->menuAction()->setVisible(false);
	}
#ifdef HAVE_PGPUTIL
	gpgAssignKeyAction_->setVisible(contact_->account()->hasPGP()
									&& PGPUtil::instance().pgpAvailable()
									&& PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool()
									&& contact_->userListItem().publicKeyID().isEmpty());

	gpgUnassignKeyAction_->setVisible(contact_->account()->hasPGP()
									  && PGPUtil::instance().pgpAvailable()
									  && PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool()
									  && !contact_->userListItem().publicKeyID().isEmpty());
#endif // HAVE_PGPUTIL
}

void PsiContactMenu::Private::mucHide()
{
	GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
	if (gc && (gc->isTabbed() || !gc->isHidden()))
		gc->hideTab();
}

void PsiContactMenu::Private::mucShow()
{
	GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
	if (gc) {
		gc->ensureTabbedCorrectly();
		gc->bringToFront();
	}
}

void PsiContactMenu::Private::mucLeave()
{
	GCMainDlg *gc = contact_->account()->findDialog<GCMainDlg*>(contact_->jid());
	if (gc)
		gc->close();
}

void PsiContactMenu::Private::rename()
{
	if (!contact_)
		return;

	menu_->model()->renameSelectedItem();
}

void PsiContactMenu::Private::addContact()
{
	emit menu_->addSelection();
}

void PsiContactMenu::Private::removeContact()
{
	emit menu_->removeSelection();
}

void PsiContactMenu::Private::inviteToGroupchat(PsiAccount* account, QString groupchat)
{
	if (!contact_)
		return;
	account->actionInvite(contact_->jid(), groupchat);
	QMessageBox::information(0, tr("Invitation"),
							 tr("Sent groupchat invitation to <b>%1</b>.").arg(contact_->name()));
}

void PsiContactMenu::Private::setContactGroup(QString group)
{
	if (!contact_)
		return;
	contact_->setGroups(QStringList() << group);
}

void PsiContactMenu::Private::block(bool )
{
	if (!contact_)
		return;

	contact_->toggleBlockedStateConfirmation();
}

void PsiContactMenu::Private::setAlwaysVisible(bool visible)
{
	if (!contact_)
		return;

	contact_->setAlwaysVisible(visible);
}

void PsiContactMenu::Private::addAuth()
{
	if (!contact_)
		return;

	if (contact_->inList())
		contact_->account()->actionAuth(contact_->jid());
	else
		contact_->account()->actionAdd(contact_->jid());

	QMessageBox::information(0, tr("Add"),
							 tr("Added/Authorized <b>%1</b> to the contact list.").arg(contact_->name()));
}

void PsiContactMenu::Private::receiveIncomingEvent()
{
	if (!contact_)
		return;
	contact_->account()->actionRecvEvent(contact_->jid());
}

void PsiContactMenu::Private::sendMessage()
{
	if (!contact_)
		return;
	contact_->account()->actionSendMessage(contact_->jid());
}

void PsiContactMenu::Private::openChat()
{
	if (!contact_)
		return;
	contact_->account()->actionOpenChat(contact_->jid());
}

#ifdef WHITEBOARDING
void PsiContactMenu::Private::openWhiteboard()
{
	if (!contact_)
		return;

	contact_->account()->actionOpenWhiteboard(contact_->jid());
}
#endif

void PsiContactMenu::Private::voiceCall()
{
	if (!contact_)
		return;
	contact_->account()->actionVoice(contact_->jid());
}

void PsiContactMenu::Private::sendFile()
{
	if (!contact_)
		return;
	contact_->account()->actionSendFile(contact_->jid());
}

void PsiContactMenu::Private::transportLogon()
{
	if (!contact_)
		return;
	contact_->account()->actionAgentSetStatus(contact_->jid(), contact_->account()->status());
}

void PsiContactMenu::Private::transportLogoff()
{
	if (!contact_)
		return;
	contact_->account()->actionAgentSetStatus(contact_->jid(), XMPP::Status::Offline);
}

void PsiContactMenu::Private::authResend()
{
	if (!contact_)
		return;
	contact_->account()->actionAuth(contact_->jid());
	QMessageBox::information(0, tr("Authorize"),
							 tr("Sent authorization to <b>%1</b>.").arg(contact_->name()));
}

void PsiContactMenu::Private::authRerequest()
{
	if (!contact_)
		return;
	contact_->account()->actionAuthRequest(contact_->jid());
	QMessageBox::information(0, tr("Authorize"),
							 tr("Rerequested authorization from <b>%1</b>.").arg(contact_->name()));
}

void PsiContactMenu::Private::authRemove()
{
	if (!contact_)
		return;

	int n = QMessageBox::information(0, tr("Remove"),
									 tr("Are you sure you want to remove authorization from <b>%1</b>?").arg(contact_->name()),
									 tr("&Yes"), tr("&No"));

	if(n == 0)
		contact_->account()->actionAuthRemove(contact_->jid());
}

void PsiContactMenu::Private::customStatus()
{
	if (!contact_)
		return;

	contact_->account()->actionSendStatus(contact_->jid());
}

void PsiContactMenu::Private::pictureAssign()
{
	if (!contact_)
		return;
	QString file = QFileDialog::getOpenFileName(0, tr("Choose an Image"), "", tr("All files (*.png *.jpg *.gif)"));
	if (!file.isNull()) {
		contact_->account()->avatarFactory()->importManualAvatar(contact_->jid(),file);
	}
}

void PsiContactMenu::Private::pictureClear()
{
	if (!contact_)
		return;
	contact_->account()->avatarFactory()->removeManualAvatar(contact_->jid());
}

void PsiContactMenu::Private::gpgAssignKey()
{
	if (!contact_)
		return;
	contact_->account()->actionAssignKey(contact_->jid());
}

void PsiContactMenu::Private::gpgUnassignKey()
{
	if (!contact_)
		return;
	contact_->account()->actionUnassignKey(contact_->jid());
}

void PsiContactMenu::Private::vcard()
{
	if (!contact_)
		return;
	contact_->account()->actionInfo(contact_->jid());
}

void PsiContactMenu::Private::history()
{
	if (!contact_)
		return;
	contact_->account()->actionHistory(contact_->jid());
}

void PsiContactMenu::Private::sendMessageTo(PsiContact*, const XMPP::Jid& jid)
{
	if (!contact_)
		return;

	contact_->account()->actionSendMessage(jid);
}

void PsiContactMenu::Private::openChatTo(PsiContact*, const XMPP::Jid& jid)
{
	if (!contact_)
		return;

	contact_->account()->actionOpenChatSpecific(jid);
}

#ifdef WHITEBOARDING
void PsiContactMenu::Private::openWhiteboardTo(PsiContact*, const XMPP::Jid& jid)
{
	if (!contact_)
		return;

	contact_->account()->actionOpenWhiteboardSpecific(jid);
}
#endif

void PsiContactMenu::Private::executeCommand(PsiContact*, const XMPP::Jid& jid)
{
	if (!contact_)
		return;

	contact_->account()->actionExecuteCommandSpecific(jid);
}

void PsiContactMenu::Private::openActiveChat(PsiContact*, const XMPP::Jid& jid)
{
	if (!contact_)
		return;

	contact_->account()->actionOpenChatSpecific(jid);
}

void PsiContactMenu::Private::copyJid()
{
	qApp->clipboard()->setText(contact_->jid().bare());
}

void PsiContactMenu::Private::updateBlockActionState()
{
	if(!contact_)
		return;
	blockAction_->setVisible(!(contact_->isPrivate()/* || contact_->isAgent()*/ || contact_->isSelf()));
	blockAction_->setEnabled(contact_->account()->isAvailable() && dynamic_cast<PsiPrivacyManager*>(contact_->account()->privacyManager())->isAvailable());
	blockAction_->setChecked(contact_->isBlocked());
	blockAction_->setText(blockAction_->isChecked() ? tr("Unblock") : tr("Block"));
}

PsiContactMenu::PsiContactMenu(PsiContact* contact, ContactListModel* model)
	: ContactListItemMenu(nullptr, model)
//	: ContactListItemMenu(contact, model)
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
