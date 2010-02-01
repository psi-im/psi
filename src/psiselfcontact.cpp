/*
 * psiselfcontact.cpp - PsiContact that represents 'self' of an account
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

#include "psiselfcontact.h"

#include "psicontactlist.h"
#include "userlist.h"
#include "psicontactmenu.h"
#include "psiaccount.h"
#include "psicon.h"

PsiSelfContact::PsiSelfContact(const UserListItem& u, PsiAccount* parent)
	: PsiContact(u, parent)
{
}

void PsiSelfContact::update(const UserListItem& u)
{
	// if (u.userResourceList().count() != userListItem().userResourceList().count())
	// 	updateParent();

	PsiContact::update(u);
}

/**
 * Self contact should be visible either if 'Show Self' option is enabled, or
 * user is currently logged in by more than one account.
 */
bool PsiSelfContact::shouldBeVisible() const
{
#ifdef YAPSI
	return false;
#else
	return PsiContact::shouldBeVisible() || account()->psi()->contactList()->showSelf() || userListItem().userResourceList().count() > 1;
#endif
}

ContactListItemMenu* PsiSelfContact::contextMenu()
{
	// PsiContactMenu* menu = new PsiContactMenu(this);
	// QStringList toDelete;
	// toDelete << "act_rename" << "act_remove" << "act_group" << "act_authorization";
	// menu->removeActions(toDelete);
	// return menu;
	return 0;
}

bool PsiSelfContact::isEditable() const
{
	return false;
}

bool PsiSelfContact::isSelf() const
{
	return true;
}
