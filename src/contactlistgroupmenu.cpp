/*
 * contactlistgroupmenu.cpp - context menu for contact list groups
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "contactlistgroupmenu.h"

#include <QPointer>
#include <QMimeData>

#include "iconaction.h"
#include "iconset.h"
#include "contactlistgroup.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "psicontactlist.h"
#include "iconaction.h"
#include "psicontact.h"
#include "psiaccount.h"

class ContactListGroupMenu::Private : public QObject
{
	Q_OBJECT

	QPointer<ContactListGroup> group;
	QAction* renameAction_;
	QAction* removeGroupAndContactsAction_;
#ifdef YAPSI
	QAction* addGroupAction_;
#endif
	QAction* sendMessageAction_;
	QAction* removeGroupWithoutContactsAction_;

public:
	Private(ContactListGroupMenu* menu, ContactListGroup* _group)
		: QObject(0)
		, group(_group)
		, menu_(menu)
	{
		connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));

		renameAction_ = new IconAction("", tr("Re&name"), menu->shortcuts("contactlist.rename"), this, "act_rename");
		connect(renameAction_, SIGNAL(triggered()), this, SLOT(rename()));

		removeGroupAndContactsAction_ = new IconAction(tr("Remove Group and Contacts"), this, "psi/remove");
#ifdef YAPSI
		removeGroupAndContactsAction_->setText(tr("&Remove"));
#endif
		removeGroupAndContactsAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
		connect(removeGroupAndContactsAction_, SIGNAL(triggered()), SLOT(removeGroupAndContacts()));

		removeGroupWithoutContactsAction_ = new IconAction(tr("Remove Group"), this, "psi/remove");
		connect(removeGroupWithoutContactsAction_, SIGNAL(triggered()), this, SLOT(removeGroupWithoutContacts()));

		sendMessageAction_ = new IconAction(tr("Send Message to Group"), this, "psi/sendMessage");
		connect(sendMessageAction_, SIGNAL(triggered()), SLOT(sendMessage()));

#ifdef YAPSI
		addGroupAction_ = new QAction(tr("&Add group..."), this);
		connect(addGroupAction_, SIGNAL(triggered()), SLOT(addGroup()));
#endif

		updateActions();

#ifdef YAPSI
		menu->addAction(renameAction_);
		menu->addAction(removeGroupAndContactsAction_);
		menu->addAction(addGroupAction_);
#else
		menu->addAction(renameAction_);
		menu->addAction(sendMessageAction_);
		menu->addSeparator();
		menu->addAction(removeGroupWithoutContactsAction_);
		menu->addAction(removeGroupAndContactsAction_);
#endif
	}

private slots:
	void updateActions()
	{
		if (!group)
			return;

		sendMessageAction_->setVisible(PsiOptions::instance()->getOption("options.ui.message.enabled").toBool());
		renameAction_->setEnabled(group->isEditable());
		removeGroupAndContactsAction_->setEnabled(group->isRemovable());
		removeGroupWithoutContactsAction_->setEnabled(group->isRemovable());
#ifdef YAPSI
		addGroupAction_->setEnabled(group->model()->contactList()->haveAvailableAccounts());
#endif
	}

	void rename()
	{
		if (!group)
			return;

		menu_->model()->renameSelectedItem();
	}

	void removeGroupAndContacts()
	{
		if (!group)
			return;

		emit menu_->removeSelection();
	}

	void removeGroupWithoutContacts()
	{
		if (!group)
			return;

		QModelIndexList indexes;
		indexes += group->model()->groupToIndex(group);
		QMimeData* data = group->model()->mimeData(indexes);
		emit menu_->removeGroupWithoutContacts(data);
		delete data;
	}

	void sendMessage()
	{
		if (!group)
			return;

		QList<PsiContact*> contacts = group->contacts();
		if (!contacts.isEmpty()) {
			QList<XMPP::Jid> list;
			foreach(PsiContact* contact, contacts) {
				list << contact->jid();
			}
			contacts.first()->account()->actionSendMessage(list);
		}
	}

#ifdef YAPSI
	void addGroup()
	{
		if (!group)
			return;

		emit menu_->addGroup();
	}
#endif

private:
	ContactListGroupMenu* menu_;
};

ContactListGroupMenu::ContactListGroupMenu(ContactListGroup* group, ContactListModel* model)
	: ContactListItemMenu(group, model)
{
	d = new Private(this, group);
}

ContactListGroupMenu::~ContactListGroupMenu()
{
	delete d;
}

#include "contactlistgroupmenu.moc"
