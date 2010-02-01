/*
 * contactlistaccountmenu.h - context menu for contact list accounts
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONTACTLISTACCOUNTMENU_H
#define CONTACTLISTACCOUNTMENU_H

#include "contactlistitemmenu.h"

class ContactListAccountGroup;

class ContactListAccountMenu : public ContactListItemMenu
{
	Q_OBJECT
public:
	ContactListAccountMenu(ContactListAccountGroup* account, ContactListModel* model);
	~ContactListAccountMenu();

private:
	class Private;
	Private* d;
};

#endif
