/*
 * contactlistaccountmenu.cpp - context menu for contact list accounts
 * Copyright (C) 2009-2010  Michail Pishchagin
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

#include "contactlistaccountmenu.h"

#include <QPointer>

#include "pluginmanager.h"
#include "psiiconset.h"
#include "psiaccount.h"
#include "accountstatusmenu.h"
#include "serverinfomanager.h"
#include "bookmarkmanager.h"
#include "psioptions.h"
#include "iconaction.h"
#include "privacy/privacydlg.h"
#include "psiiconset.h"
#include "common.h"
#include "contactmanager/contactmanagerdlg.h"
#include "mucjoindlg.h"

class ContactListAccountMenu::Private : public QObject
{
	Q_OBJECT

	PsiAccount *account;
	AccountStatusMenu* statusMenu_;
	QAction* moodAction_;
	QAction* activityAction_;
	QAction* geolocationAction_;
	QAction* setAvatarAction_;
	QMenu* avatarMenu_;
	QAction* unsetAvatarAction_;
	QAction* contactManagerAction_;
	QMenu* bookmarksMenu_;
	QAction* bookmarksManageAction_;
	QList<QAction*> bookmarksJoinActions_;
	QAction* addContactAction_;
	QAction* serviceDiscoveryAction_;
	QAction* newMessageAction_;
	QAction* xmlConsoleAction_;
	QAction* privacyListsAction_;
	QAction* modifyAccountAction_;
	QMenu* adminMenu_;
	QAction* adminOnlineUsersAction_;
	QAction* adminSendServerMessageAction_;
	QAction* adminSetMotdAction_;
	QAction* adminUpdateMotdAction_;
	QAction* adminDeleteMotdAction_;
	QAction *doGroupChatAction_;
	QMenu* pluginsMenu_;

public:
	Private(ContactListAccountMenu *menu, PsiAccount *_account)
		: QObject(0)
		, account(_account)
	{
		connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));
		connect(account, SIGNAL(updatedActivity()), SLOT(updateActions()));
		connect(account, SIGNAL(updatedAccount()), SLOT(updateActions()));

		statusMenu_ = new AccountStatusMenu(0, account->psi(), account);
		statusMenu_->fill();
		statusMenu_->setTitle(tr("&Status"));
		statusMenu_->setIcon(PsiIconset::instance()->status(makeSTATUS(account->status())).icon());
		connect(statusMenu_, SIGNAL(statusSelected(XMPP::Status::Type, bool)), SLOT(statusChanged(XMPP::Status::Type, bool)));
		connect(statusMenu_, SIGNAL(statusPresetSelected(const XMPP::Status &, bool, bool)), account, SLOT(setStatus(const XMPP::Status &, bool, bool)));
		connect(statusMenu_, SIGNAL(statusPresetDialogForced(const QString &)), account, SLOT(showStatusDialog(const QString &)));
		connect(statusMenu_, SIGNAL(reconnectActivated()), account, SLOT(reconnectOnce()));

		moodAction_ = new IconAction(tr("Mood"), this, QString(("mood/%1")).arg(account->mood().typeValue()));
		connect(moodAction_, SIGNAL(triggered()), SLOT(setMood()));

		QString act = account->activity().typeValue();
		if (account->activity().specificType() != Activity::UnknownSpecific && account->activity().specificType() != Activity::Other) {
			act += "_" + account->activity().specificTypeValue();
		}
		activityAction_ = new IconAction(tr("Activity"), this, QString(("activities/%1")).arg(act));
		connect(activityAction_, SIGNAL(triggered()), SLOT(setActivity()));

		geolocationAction_ = new IconAction(tr("GeoLocation"), this, "pep/geolocation");
		connect(geolocationAction_, SIGNAL(triggered()), SLOT(setGeolocation()));

		setAvatarAction_ = new QAction(tr("Set Avatar"), this);
		connect(setAvatarAction_, SIGNAL(triggered()), SLOT(setAvatar()));

		unsetAvatarAction_ = new QAction(tr("Unset Avatar"), this);
		connect(unsetAvatarAction_, SIGNAL(triggered()), SLOT(unsetAvatar()));

		contactManagerAction_ = new IconAction(tr("&Contacts Manager"), this, "psi/action_contacts_manager");
		connect(contactManagerAction_, SIGNAL(triggered()), SLOT(contactManager()));

		bookmarksManageAction_ = new QAction(tr("Manage Bookmarks"), this);
		connect(bookmarksManageAction_, SIGNAL(triggered()), SLOT(bookmarksManage()));

		doGroupChatAction_ = new IconAction(tr("Join Groupchat"), this, "psi/groupChat");
		connect(doGroupChatAction_, SIGNAL(triggered()), SLOT(doGroupChat()));

		addContactAction_ = new IconAction(tr("&Add a Contact"), this, "psi/addContact");
		connect(addContactAction_, SIGNAL(triggered()), SLOT(addContact()));

		serviceDiscoveryAction_ = new IconAction(tr("Service &Discovery"), this, "psi/disco");
		connect(serviceDiscoveryAction_, SIGNAL(triggered()), SLOT(serviceDiscovery()));

		newMessageAction_ = new IconAction(tr("New &Blank Message"), this, "psi/sendMessage");
		connect(newMessageAction_, SIGNAL(triggered()), SLOT(newMessage()));

		privacyListsAction_ = new IconAction(tr("Privacy Lists"), this, "psi/eye");
		connect(privacyListsAction_, SIGNAL(triggered()), SLOT(privacyLists()));

		xmlConsoleAction_ = new IconAction(tr("&XML Console"), this, "psi/xml");
		connect(xmlConsoleAction_, SIGNAL(triggered()), SLOT(xmlConsole()));

		modifyAccountAction_ = new IconAction(tr("&Modify Account..."), this, "psi/account");
		connect(modifyAccountAction_, SIGNAL(triggered()), SLOT(modifyAccount()));

		adminOnlineUsersAction_ = new IconAction(tr("Online Users"), this, "psi/disco");
		connect(adminOnlineUsersAction_, SIGNAL(triggered()), SLOT(adminOnlineUsers()));

		adminSendServerMessageAction_ = new IconAction(tr("Send Server Message"), this, "psi/sendMessage");
		connect(adminSendServerMessageAction_, SIGNAL(triggered()), SLOT(adminSendServerMessage()));

		adminSetMotdAction_ = new QAction(tr("Set MOTD"), this);
		connect(adminSetMotdAction_, SIGNAL(triggered()), SLOT(adminSetMotd()));

		adminUpdateMotdAction_ = new QAction(tr("Update MOTD"), this);
		connect(adminUpdateMotdAction_, SIGNAL(triggered()), SLOT(adminUpdateMotd()));

		adminDeleteMotdAction_ = new IconAction(tr("Delete MOTD"), this, "psi/remove");
		connect(adminDeleteMotdAction_, SIGNAL(triggered()), SLOT(adminDeleteMotd()));

		menu->addMenu(statusMenu_);
		menu->addAction(moodAction_);
		menu->addAction(activityAction_);
		menu->addAction(geolocationAction_);
		avatarMenu_ = menu->addMenu(IconsetFactory::icon("psi/vCard").icon(), tr("Avatar"));
		avatarMenu_->addAction(setAvatarAction_);
		avatarMenu_->addAction(unsetAvatarAction_);
		bookmarksMenu_ = menu->addMenu(IconsetFactory::icon("psi/bookmarks").icon(), tr("Groupchat"));
		bookmarksMenu_->addAction(doGroupChatAction_);
		bookmarksMenu_->addSeparator();
		bookmarksMenu_->addAction(bookmarksManageAction_);
		menu->addSeparator();
		menu->addAction(addContactAction_);
		menu->addAction(serviceDiscoveryAction_);
		menu->addAction(newMessageAction_);
		menu->addAction(contactManagerAction_);
		menu->addAction(privacyListsAction_);
		menu->addSeparator();
		menu->addAction(xmlConsoleAction_);
		menu->addSeparator();
		menu->addAction(modifyAccountAction_);

#ifdef PSI_PLUGINS
		pluginsMenu_ = menu->addMenu(IconsetFactory::icon("psi/plugins").icon(), tr("Plugins"));
		PluginManager::instance()->addAccountMenu(pluginsMenu_, account);
#endif

		adminMenu_ = menu->addMenu(tr("&Admin"));
		adminMenu_->addAction(adminOnlineUsersAction_);
		adminMenu_->addAction(adminSendServerMessageAction_);
		adminMenu_->addAction(adminSetMotdAction_);
		adminMenu_->addAction(adminUpdateMotdAction_);
		adminMenu_->addAction(adminDeleteMotdAction_);

		updateActions();
	}

	~Private()
	{
		delete statusMenu_;
	}

private slots:
	void updateActions()
	{
		if (!account)
			return;

		statusMenu_->statusChanged(account->status());
#ifndef USE_PEP
		moodAction_->setVisible(false);
		activityAction_->setVisible(false);
		geolocationAction_->setVisible(false);
		avatarMenu_->setVisible(false);
#else
		moodAction_->setEnabled(account->serverInfoManager()->hasPEP());
		activityAction_->setEnabled(account->serverInfoManager()->hasPEP());
		geolocationAction_->setEnabled(account->serverInfoManager()->hasPEP());
		avatarMenu_->setEnabled(account->serverInfoManager()->hasPEP());
#endif
		bookmarksMenu_->clear();
		qDeleteAll(bookmarksJoinActions_);
		bookmarksJoinActions_.clear();
		bookmarksMenu_->addAction(doGroupChatAction_);
		bookmarksMenu_->addSeparator();
		bookmarksMenu_->addAction(bookmarksManageAction_);
		bookmarksMenu_->setEnabled(true);
		if (account->bookmarkManager()->isAvailable()) {
			bookmarksManageAction_->setEnabled(true);
			bookmarksMenu_->addSeparator();
			foreach(ConferenceBookmark c, account->bookmarkManager()->conferences()) {
				QAction* joinAction = new QAction(QString(tr("Join %1")).arg(c.name()), this);
				joinAction->setProperty("bookmark", bookmarksJoinActions_.count());
				connect(joinAction, SIGNAL(triggered()), SLOT(bookmarksJoin()));
				bookmarksMenu_->addAction(joinAction);
				bookmarksJoinActions_ << joinAction;
			}
		}
		else {
			bookmarksManageAction_->setEnabled(false);
		}

		newMessageAction_->setVisible(true);
		newMessageAction_->setEnabled(account->isAvailable());
		addContactAction_->setEnabled(account->isAvailable());
		serviceDiscoveryAction_->setEnabled(account->isAvailable());
		contactManagerAction_->setEnabled(account->isAvailable());
		privacyListsAction_->setEnabled(account->isAvailable());
		if (!PsiOptions::instance()->getOption("options.ui.menu.account.admin").toBool()) {
			adminMenu_->menuAction()->setVisible(false);
		}
		adminMenu_->setEnabled(account->isAvailable());
		adminSendServerMessageAction_->setVisible(newMessageAction_->isVisible());
		adminSetMotdAction_->setVisible(newMessageAction_->isVisible());
		adminUpdateMotdAction_->setVisible(newMessageAction_->isVisible());
		adminDeleteMotdAction_->setVisible(newMessageAction_->isVisible());

#ifdef PSI_PLUGINS
		if(pluginsMenu_->isEmpty())
			pluginsMenu_->menuAction()->setVisible(false);
		pluginsMenu_->setEnabled(account->isAvailable() && !pluginsMenu_->isEmpty());
#endif
	}

	void statusChanged(XMPP::Status::Type statusType, bool forceDialog)
	{
		if (!account)
			return;

		account->changeStatus(static_cast<int>(statusType), forceDialog);
	}

	void setMood()
	{
		if (!account)
			return;

		account->actionSetMood();
	}

	void setActivity()
	{
		if (!account)
			return;

		account->actionSetActivity();
	}

	void setGeolocation()
	{
		if (!account)
			return;

		account->actionSetGeoLocation();
	}

	void setAvatar()
	{
		if (!account)
			return;

		account->actionSetAvatar();
	}

	void unsetAvatar()
	{
		if (!account)
			return;

		account->actionUnsetAvatar();
	}

	void bookmarksManage()
	{
		if (!account)
			return;

		account->actionManageBookmarks();
	}

	void doGroupChat()
	{
		if (!account)
			return;

		MUCJoinDlg *w = new MUCJoinDlg(account->psi(), account);
		w->show();
	}

	void bookmarksJoin()
	{
		if (!account)
			return;

		QAction* joinAction = static_cast<QAction*>(sender());
		ConferenceBookmark c = account->bookmarkManager()->conferences()[joinAction->property("bookmark").toInt()];
		account->actionJoin(c, true);
	}

	void addContact()
	{
		if (!account)
			return;

		account->openAddUserDlg();
	}

	void serviceDiscovery()
	{
		if (!account)
			return;

		XMPP::Jid j = account->jid().domain();
		account->actionDisco(j, "");
	}

	void newMessage()
	{
		if (!account)
			return;

		account->actionSendMessage("");
	}

	void privacyLists()
	{
		if (!account)
			return;

		PrivacyDlg *dlg = account->findDialog<PrivacyDlg*>();
		if(!dlg) {
			dlg = new PrivacyDlg(account->name(), account->privacyManager());
			account->dialogRegister(dlg);
			dlg->show();
		} else
			bringToFront(dlg);
	}

	void contactManager()
	{
		if (!account)
			return;

		ContactManagerDlg *dlg = account->findDialog<ContactManagerDlg*>();
		if(!dlg) {
			dlg = new ContactManagerDlg(account);
			dlg->show();
		} else
			bringToFront(dlg);
	}

	void xmlConsole()
	{
		if (!account)
			return;

		account->showXmlConsole();
	}

	void modifyAccount()
	{
		if (!account)
			return;

		account->modify();
	}

	void adminOnlineUsers()
	{
		if (!account)
			return;

		// FIXME: will it still work on XMPP servers?
		XMPP::Jid j = account->jid().domain() + '/' + "admin";
		account->actionDisco(j, "");
	}

	void adminSendServerMessage()
	{
		if (!account)
			return;

		XMPP::Jid j = account->jid().domain() + '/' + "announce/online";
		account->actionSendMessage(j);
	}

	void adminSetMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->jid().domain() + '/' + "announce/motd";
		account->actionSendMessage(j);
	}

	void adminUpdateMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->jid().domain() + '/' + "announce/motd/update";
		account->actionSendMessage(j);
	}

	void adminDeleteMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->jid().domain() + '/' + "announce/motd/delete";
		account->actionSendMessage(j);
	}

};

ContactListAccountMenu::ContactListAccountMenu(PsiAccount *account, ContactListModel *model)
	: ContactListItemMenu(nullptr, model)
{
	d = new Private(this, account);
	setLabelTitle(account->name());
}

ContactListAccountMenu::~ContactListAccountMenu()
{
	delete d;
}

#include "contactlistaccountmenu.moc"
