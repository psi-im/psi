/*
 * psicontactmenu.h - a PsiContact context menu
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

#ifndef PSICONTACTMENU_H
#define PSICONTACTMENU_H

#include "contactlistitemmenu.h"

class PsiContact;

class PsiContactMenu : public ContactListItemMenu
{
	Q_OBJECT
public:
	PsiContactMenu(PsiContact* contact, ContactListModel* model);
	~PsiContactMenu();

	// reimplemented
	virtual QList<QAction*> availableActions() const;

signals:
	void removeSelection();
	void addSelection();

private:
	class Private;
	friend class Private;
	Private* d;
};

#endif
