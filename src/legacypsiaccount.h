/*
 * legacypsiaccount.h - PsiAccount with ContactProfile compatibility
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

#ifndef LEGACYPSIACCOUNT_H
#define LEGACYPSIACCOUNT_H

#include "psiaccount.h"

class LegacyPsiAccount : public PsiAccount
{
	Q_OBJECT
public:
	LegacyPsiAccount(const UserAccount &acc, PsiContactList *parent, CapsRegistry* capsRegistry, TabManager *tabManager);
	~LegacyPsiAccount();
	virtual void init();

	// reimplemented
	virtual ContactProfile* contactProfile() const;
	virtual void setUserAccount(const UserAccount &);
	virtual void stateChanged();

	// reimplemented
	virtual void profileUpdateEntry(const UserListItem& u);
	virtual void profileRemoveEntry(const Jid& jid);
	virtual void profileAnimateNick(const Jid& jid);
	virtual void profileSetAlert(const Jid& jid, const PsiIcon* icon);
	virtual void profileClearAlert(const Jid& jid);

private:
	ContactProfile* cp_;
};

#endif
