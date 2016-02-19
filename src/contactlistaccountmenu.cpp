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

#include "psiaccount.h"
#include "contactlistaccountgroup.h"
#include "statusmenu.h"
#include "serverinfomanager.h"
#include "bookmarkmanager.h"
#include "psioptions.h"
#include "iconaction.h"
#include "privacy/privacydlg.h"
#include "iconset.h"
#include "common.h"

class ContactListAccountMenu::Private : public QObject
{
	Q_OBJECT

	QPointer<ContactListAccountGroup> account;
	StatusMenu* statusMenu_;
	QAction* moodAction_;
	QAction* activityAction_;
	QAction* geolocationAction_;
	QAction* setAvatarAction_;
	QMenu* avatarMenu_;
	QAction* unsetAvatarAction_;
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

public:
	Private(ContactListAccountMenu* menu, ContactListAccountGroup* _account)
		: QObject(0)
		, account(_account)
		, menu_(menu)
	{
		connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));
		connect(account->account(), SIGNAL(updatedActivity()), SLOT(updateActions()));
		connect(account->account(), SIGNAL(updatedAccount()), SLOT(updateActions()));

		statusMenu_ = new StatusMenu(0);
		statusMenu_->setTitle(tr("&Status"));
		connect(statusMenu_, SIGNAL(statusChanged(XMPP::Status::Type)), SLOT(statusChanged(XMPP::Status::Type)));

		moodAction_ = new QAction(tr("Mood"), this);
		connect(moodAction_, SIGNAL(triggered()), SLOT(setMood()));

		activityAction_ = new QAction(tr("Activity"), this);
		connect(activityAction_, SIGNAL(triggered()), SLOT(setActivity()));

		geolocationAction_ = new QAction(tr("GeoLocation"), this);
		connect(geolocationAction_, SIGNAL(triggered()), SLOT(setGeolocation()));

		setAvatarAction_ = new QAction(tr("Set Avatar"), this);
		connect(setAvatarAction_, SIGNAL(triggered()), SLOT(setAvatar()));

		unsetAvatarAction_ = new QAction(tr("Unset Avatar"), this);
		connect(unsetAvatarAction_, SIGNAL(triggered()), SLOT(unsetAvatar()));

		bookmarksManageAction_ = new QAction(tr("Manage..."), this);
		connect(bookmarksManageAction_, SIGNAL(triggered()), SLOT(bookmarksManage()));

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
		avatarMenu_ = menu->addMenu(tr("Avatar"));
		avatarMenu_->addAction(setAvatarAction_);
		avatarMenu_->addAction(unsetAvatarAction_);
		bookmarksMenu_ = menu->addMenu(IconsetFactory::icon("psi/bookmarks").icon(), tr("Bookmarks"));
		bookmarksMenu_->addAction(bookmarksManageAction_);
		menu->addSeparator();
		menu->addAction(addContactAction_);
		menu->addAction(serviceDiscoveryAction_);
		menu->addAction(newMessageAction_);
		menu->addAction(privacyListsAction_);
		menu->addSeparator();
		menu->addAction(xmlConsoleAction_);
		menu->addSeparator();
		menu->addAction(modifyAccountAction_);
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

		statusMenu_->setStatus(account->account()->status().type());
#ifndef USE_PEP
		moodAction_->setVisible(false);
		activityAction_->setVisible(false);
		geolocationAction_->setVisible(false);
		avatarMenu_->setVisible(false);
#else
		moodAction_->setEnabled(account->account()->serverInfoManager()->hasPEP());
		activityAction_->setEnabled(account->account()->serverInfoManager()->hasPEP());
		geolocationAction_->setEnabled(account->account()->serverInfoManager()->hasPEP());
		avatarMenu_->setEnabled(account->account()->serverInfoManager()->hasPEP());
#endif
		bookmarksMenu_->clear();
		qDeleteAll(bookmarksJoinActions_);
		bookmarksJoinActions_.clear();
		bookmarksMenu_->addAction(bookmarksManageAction_);
		if (account->account()->bookmarkManager()->isAvailable()) {
			bookmarksMenu_->setEnabled(true);
			bookmarksMenu_->addSeparator();
			foreach(ConferenceBookmark c, account->account()->bookmarkManager()->conferences()) {
				QAction* joinAction = new QAction(QString(tr("Join %1")).arg(c.name()), this);
				joinAction->setProperty("bookmark", bookmarksJoinActions_.count());
				connect(joinAction, SIGNAL(triggered()), SLOT(bookmarksJoin()));
				bookmarksMenu_->addAction(joinAction);
				bookmarksJoinActions_ << joinAction;
			}
		}
		else {
			bookmarksMenu_->setEnabled(false);
		}

		newMessageAction_->setVisible(PsiOptions::instance()->getOption("options.ui.message.enabled").toBool());
		newMessageAction_->setEnabled(account->account()->isAvailable());
		addContactAction_->setEnabled(account->account()->isAvailable());
		serviceDiscoveryAction_->setEnabled(account->account()->isAvailable());
		privacyListsAction_->setEnabled(account->account()->isAvailable());
		if (!PsiOptions::instance()->getOption("options.ui.menu.account.admin").toBool()) {
			adminMenu_->menuAction()->setVisible(false);
		}
		adminMenu_->setEnabled(account->account()->isAvailable());
		adminSendServerMessageAction_->setVisible(newMessageAction_->isVisible());
		adminSetMotdAction_->setVisible(newMessageAction_->isVisible());
		adminUpdateMotdAction_->setVisible(newMessageAction_->isVisible());
		adminDeleteMotdAction_->setVisible(newMessageAction_->isVisible());
	}

	void statusChanged(XMPP::Status::Type statusType)
	{
		if (!account)
			return;

		account->account()->changeStatus(static_cast<int>(statusType));
	}

	void setMood()
	{
		if (!account)
			return;

		account->account()->actionSetMood();
	}

	void setActivity()
	{
		if (!account)
			return;

		account->account()->actionSetActivity();
	}

	void setGeolocation()
	{
		if (!account)
			return;

		account->account()->actionSetGeoLocation();
	}

	void setAvatar()
	{
		if (!account)
			return;

		account->account()->actionSetAvatar();
	}

	void unsetAvatar()
	{
		if (!account)
			return;

		account->account()->actionUnsetAvatar();
	}

	void bookmarksManage()
	{
		if (!account)
			return;

		account->account()->actionManageBookmarks();
	}

	void bookmarksJoin()
	{
		if (!account)
			return;

		QAction* joinAction = static_cast<QAction*>(sender());
		ConferenceBookmark c = account->account()->bookmarkManager()->conferences()[joinAction->property("bookmark").toInt()];
		account->account()->actionJoin(c, true);
	}

	void addContact()
	{
		if (!account)
			return;

		account->account()->openAddUserDlg();
	}

	void serviceDiscovery()
	{
		if (!account)
			return;

		XMPP::Jid j = account->account()->jid().domain();
		account->account()->actionDisco(j, "");
	}

	void newMessage()
	{
		if (!account)
			return;

		account->account()->actionSendMessage("");
	}

	void privacyLists()
	{
		if (!account)
			return;

		PrivacyDlg *dlg = account->account()->findDialog<PrivacyDlg*>();
		if(!dlg) {
			dlg = new PrivacyDlg(account->account()->name(), account->account()->privacyManager());
			account->account()->dialogRegister(dlg);
			dlg->show();
		} else
			bringToFront(dlg);
	}

	void xmlConsole()
	{
		if (!account)
			return;

		account->account()->showXmlConsole();
	}

	void modifyAccount()
	{
		if (!account)
			return;

		account->account()->modify();
	}

	void adminOnlineUsers()
	{
		if (!account)
			return;

		// FIXME: will it still work on XMPP servers?
		XMPP::Jid j = account->account()->jid().domain() + '/' + "admin";
		account->account()->actionDisco(j, "");
	}

	void adminSendServerMessage()
	{
		if (!account)
			return;

		XMPP::Jid j = account->account()->jid().domain() + '/' + "announce/online";
		account->account()->actionSendMessage(j);
	}

	void adminSetMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->account()->jid().domain() + '/' + "announce/motd";
		account->account()->actionSendMessage(j);
	}

	void adminUpdateMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->account()->jid().domain() + '/' + "announce/motd/update";
		account->account()->actionSendMessage(j);
	}

	void adminDeleteMotd()
	{
		if (!account)
			return;

		XMPP::Jid j = account->account()->jid().domain() + '/' + "announce/motd/delete";
		account->account()->actionSendMessage(j);
	}

private:
	ContactListAccountMenu* menu_;
};

ContactListAccountMenu::ContactListAccountMenu(ContactListAccountGroup* account, ContactListModel* model)
	: ContactListItemMenu(account, model)
{
	d = new Private(this, account);
}

ContactListAccountMenu::~ContactListAccountMenu()
{
	delete d;
}

#include "contactlistaccountmenu.moc"
