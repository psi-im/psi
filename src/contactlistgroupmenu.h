/*
 * contactlistgroupmenu.h - context menu for contact list groups
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

#ifndef CONTACTLISTGROUPMENU_H
#define CONTACTLISTGROUPMENU_H

#include "contactlistitemmenu.h"

class ContactListGroup;
class QMimeData;

class ContactListGroupMenu : public ContactListItemMenu
{
	Q_OBJECT
public:
	ContactListGroupMenu(ContactListGroup* group, ContactListModel* model);
	~ContactListGroupMenu();

signals:
	void removeSelection();
	void removeGroupWithoutContacts(QMimeData*);
#ifdef YAPSI
	void addGroup();
#endif

private:
	class Private;
	Private* d;
};

#endif
