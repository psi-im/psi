/*
 * psiselfcontact.h - PsiContact that represents 'self' of an account
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

#ifndef PSISELFCONTACT_H
#define PSISELFCONTACT_H

#include "psicontact.h"

class PsiSelfContact : public PsiContact
{
public:
	PsiSelfContact(const UserListItem& u, PsiAccount* parent);

	void update(const UserListItem& u);

	// reimplemented
	virtual ContactListItemMenu* contextMenu();
	virtual bool isEditable() const;
	virtual bool isSelf() const;

protected:
	// reimplemented
	virtual bool shouldBeVisible() const;
};

#endif
