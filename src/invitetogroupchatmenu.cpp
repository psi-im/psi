/*
 * invitetogroupchatmenu.cpp - invite to groupchat context menu option
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
 *
 * This file is part of the WhoerIM project.
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

#include "invitetogroupchatmenu.h"
#include "psicontact.h"
#include "psiaccount.h"
#include "psicon.h"

InviteToGroupChatMenu::InviteToGroupChatMenu(QWidget* parent)
	: QMenu(parent)
	, controller_(0)
{
}

void InviteToGroupChatMenu::updateMenu(PsiContact* contact)
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

void InviteToGroupChatMenu::actionActivated()
{
	QAction* action = static_cast<QAction*>(sender());
	emit inviteToGroupchat(controller_->contactList()->getAccount(action->property("account").toString()),
						   action->property("groupChat").toString());
}
