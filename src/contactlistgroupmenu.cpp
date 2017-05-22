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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactlistgroupmenu_p.h"

#include <QMessageBox>

/*********************************/
/* ContactListGroupMenu::Private */
/*********************************/


ContactListGroupMenu::Private::Private(ContactListGroupMenu *menu, ContactListItem *item)
	: QObject(0)
	, q(menu)
	, group(item)
{
	q->setLabelTitle(item->name());

	connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));

	renameAction_ = new IconAction("", tr("Re&name"), menu->shortcuts("contactlist.rename"), this, "act_rename");
	connect(renameAction_, SIGNAL(triggered()), this, SLOT(rename()));

	actionAuth_ = new QAction(tr("Resend Authorization to Group"), this);
	connect(actionAuth_, SIGNAL(triggered()), this, SLOT(authResend()));

	actionAuthRequest_ = new QAction(tr("Request Authorization from Group"), this);
	connect(actionAuthRequest_, SIGNAL(triggered()), this, SLOT(authRequest()));

	actionAuthRemove_ = new QAction(tr("Remove Authorization from Group"), this);
	connect(actionAuthRemove_, SIGNAL(triggered()), this, SLOT(authRemove()));

	actionCustomStatus_ = new IconAction(tr("Send Status to Group"), this, "psi/action_direct_presence");
	connect(actionCustomStatus_, SIGNAL(triggered()), this, SLOT(customStatus()));

	removeGroupAndContactsAction_ = new IconAction(tr("Remove Group and Contacts"), this, "psi/remove");
	removeGroupAndContactsAction_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
	connect(removeGroupAndContactsAction_, SIGNAL(triggered()), SLOT(removeGroupAndContacts()));

	removeGroupWithoutContactsAction_ = new IconAction(tr("Remove Group"), this, "psi/remove");
	connect(removeGroupWithoutContactsAction_, SIGNAL(triggered()), this, SLOT(removeGroupWithoutContacts()));

	sendMessageAction_ = new IconAction(tr("Send Message to Group"), this, "psi/sendMessage");
	connect(sendMessageAction_, SIGNAL(triggered()), SLOT(sendMessage()));

	actionMucHide_ = new IconAction(tr("Hide All"), this, "psi/action_muc_hide");
	connect(actionMucHide_, SIGNAL(triggered()), SLOT(mucHide()));

	actionMucShow_ = new IconAction(tr("Show All"), this, "psi/action_muc_show");
	connect(actionMucShow_, SIGNAL(triggered()), SLOT(mucShow()));

	actionMucLeave_ = new IconAction(tr("Leave All"), this, "psi/action_muc_leave");
	connect(actionMucLeave_, SIGNAL(triggered()), SLOT(mucLeave()));

	actionHide_ = new IconAction(tr("Hidden"), "psi/show_hidden", tr("Hidden"), 0, this, 0, true);
	connect(actionHide_, SIGNAL(triggered(bool)), SLOT(actHide(bool)));

	authMenu_ = 0;

	if (item->specialGroupType() != ContactListItem::SpecialGroupType::ConferenceSpecialGroupType) {
		q->addAction(renameAction_);
		q->addAction(sendMessageAction_);
		q->addAction(actionCustomStatus_);
		q->addSeparator();
		q->addAction(actionHide_);
		q->addSeparator();
		authMenu_ = menu->addMenu(tr("Authorization"));
		authMenu_->addAction(actionAuth_);
		authMenu_->addAction(actionAuthRequest_);
		authMenu_->addAction(actionAuthRemove_);
		menu->addAction(removeGroupWithoutContactsAction_);
		menu->addAction(removeGroupAndContactsAction_);
	}
	else {
		menu->addAction(actionMucHide_);
		menu->addAction(actionMucShow_);
		menu->addAction(actionMucLeave_);
		menu->addSeparator();
		menu->addAction(actionCustomStatus_);
	}
	updateActions();
}

void ContactListGroupMenu::Private::updateActions()
{
	if (!group || group->contacts().isEmpty())
		return;

	sendMessageAction_->setVisible(true);
	sendMessageAction_->setEnabled(group->contacts().first()->account()->isAvailable());
	actionCustomStatus_->setEnabled(group->contacts().first()->account()->isAvailable());
	if(authMenu_)
		authMenu_->setEnabled(group->contacts().first()->account()->isAvailable());
	renameAction_->setEnabled(group->isEditable());
	removeGroupAndContactsAction_->setEnabled(group->isRemovable());
	removeGroupWithoutContactsAction_->setEnabled(group->isRemovable());
	actionHide_->setChecked(group->isHidden());
}

void ContactListGroupMenu::Private::mucHide()
{
	PsiAccount *pa = group->contacts().first()->account();
	foreach (QString gc, pa->groupchats()) {
		Jid j(gc);
		GCMainDlg *gcDlg = pa->findDialog<GCMainDlg*>(j.bare());
		if (gcDlg && (gcDlg->isTabbed() || !gcDlg->isHidden()))
			gcDlg->hideTab();
	}
}

void ContactListGroupMenu::Private::mucShow()
{
	PsiAccount *pa = group->contacts().first()->account();
	foreach (QString gc, pa->groupchats()) {
		Jid j(gc);
		GCMainDlg *gcDlg = pa->findDialog<GCMainDlg*>(j.bare());
		if (gcDlg) {
			gcDlg->ensureTabbedCorrectly();
			gcDlg->bringToFront();
		}
	}
}

void ContactListGroupMenu::Private::mucLeave()
{
	PsiAccount *pa = group->contacts().first()->account();
	foreach (QString gc, pa->groupchats()) {
		Jid j(gc);
		GCMainDlg *gcDlg = pa->findDialog<GCMainDlg*>(j.bare());
		if (gcDlg)
			gcDlg->close();
	}
}

void ContactListGroupMenu::Private::rename()
{
	if (!group)
		return;

	q->model()->renameSelectedItem();
}

void ContactListGroupMenu::Private::removeGroupAndContacts()
{
	if (!group)
		return;

	emit q->removeSelection();
}

void ContactListGroupMenu::Private::authResend()
{
	if (!group)
		return;

	QList<PsiContact*> contacts = group->contacts();
	if (!contacts.isEmpty()) {
		foreach(PsiContact* contact, contacts) {
			contact->account()->actionAuth(contact->jid());
		}
	}
}

void ContactListGroupMenu::Private::authRequest()
{
	if (!group)
		return;

	QList<PsiContact*> contacts = group->contacts();
	if (!contacts.isEmpty()) {
		foreach(PsiContact* contact, contacts) {
			contact->account()->actionAuthRequest(contact->jid());
		}
	}
}

void ContactListGroupMenu::Private::authRemove()
{
	if (!group)
		return;

	QList<PsiContact*> contacts = group->contacts();
	if (!contacts.isEmpty()) {
		foreach(PsiContact* contact, contacts) {
			contact->account()->actionAuthRemove(contact->jid());
		}
	}
}

void ContactListGroupMenu::Private::customStatus()
{
	if (!group)
		return;

	PsiAccount *pa = group->contacts().first()->account();
	StatusSetDlg *w = new StatusSetDlg(pa->psi(), makeLastStatus(pa->status().type()), lastPriorityNotEmpty());
	QList<XMPP::Jid> list;
	foreach(PsiContact* contact, group->contacts()) {
		if(contact->isPrivate()) continue;
		list << contact->jid();
	}
	w->setJidList(list);
	connect(w, SIGNAL(setJidList(const QList<XMPP::Jid> &, const Status &)), SLOT(setStatusFromDialog(const QList<XMPP::Jid> &, const Status &)));
	w->show();
}

void ContactListGroupMenu::Private::setStatusFromDialog(const QList<XMPP::Jid> &j, const Status &s)
{
	PsiAccount *pa = group->contacts().first()->account();
	for(QList<Jid>::const_iterator it = j.begin(); it != j.end(); ++it)
	{
		JT_Presence *p = new JT_Presence(pa->client()->rootTask());
		p->pres(*it,s);
		p->go(true);
	}
}

void ContactListGroupMenu::Private::removeGroupWithoutContacts()
{
	if (!group)
		return;

	QList<PsiContact*> contacts = group->contacts();

	if (contacts.isEmpty())
		return;

	int res = QMessageBox::information(q, tr("Remove Group"),
									   tr("This will cause all contacts in this group to be disassociated with it.\n"
										  "\n"
										  "Proceed?"),
									   QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);

	if (res == QMessageBox::StandardButton::Yes) {
		for (PsiContact *contact: contacts) {
			QStringList groups = contact->groups();
			groups.removeAll(group->name());
			contact->setGroups(groups);
		}
	}
}

void ContactListGroupMenu::Private::sendMessage()
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

void ContactListGroupMenu::Private::actHide(bool hide)
{
	if (!group)
		return;

	group->setHidden(hide);
}

/************************/
/* ContactListGroupMenu */
/************************/

ContactListGroupMenu::ContactListGroupMenu(ContactListItem *item, ContactListModel* model)
	: ContactListItemMenu(item, model)
{
	d = new Private(this, item);
}

ContactListGroupMenu::~ContactListGroupMenu()
{
	delete d;
}
