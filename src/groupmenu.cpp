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

#include "groupmenu.h"

#include "userlist.h"
#include "psicontact.h"
#include "psiaccount.h"

#include <QInputDialog>

GroupMenu::GroupMenu(QWidget* parent)
	: QMenu(parent)
{
}

void GroupMenu::updateMenu(PsiContact* contact)
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

/**
 * \param text will be shown on screen, and \param groupName is the
 * actual group name. Specify true as \param current when group is
 * currently selected for a contact.
 */
void GroupMenu::addGroup(QString text, QString groupName, bool selected)
{
	QAction* action = new QAction(text, this);
	addAction(action);
	action->setCheckable(true);
	action->setChecked(selected);
	action->setProperty("groupName", QVariant(groupName));
	connect(action, SIGNAL(triggered()), SLOT(actionActivated()));
}

void GroupMenu::actionActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	emit groupActivated(action->property("groupName").toString());
}

void GroupMenu::createNewGroup()
{
	while (contact_) {
		bool ok = false;
		QString newgroup = QInputDialog::getText(0, tr("Create New Group"),
												 tr("Enter the new group name:"),
												 QLineEdit::Normal,
												 QString(),
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
