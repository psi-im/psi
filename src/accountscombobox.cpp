/*
 * accountscombobox.cpp
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "psicon.h"
#include "accountscombobox.h"
#include "psiaccount.h"
#include "psicontactlist.h"

AccountsComboBox::AccountsComboBox(PsiCon *_psi, QWidget *parent, bool online_only)
:QComboBox(parent), onlineOnly(online_only)
{
	psi = _psi;
	// TODO: Status changes of accounts should be notified when onlineOnly is true
	connect(psi, SIGNAL(accountCountChanged()), this, SLOT(updateAccounts()));
	connect(psi, SIGNAL(destroyed()), SLOT(deleteMe()));
	connect(this, SIGNAL(activated(int)), this, SLOT(changeAccount()));
	if (online_only)
		connect(psi, SIGNAL(accountActivityChanged()), this, SLOT(updateAccounts()));

	pa = 0;
	setSizePolicy(QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed ));

	if(psi->contactList()->haveEnabledAccounts())
		setAccount(psi->contactList()->enabledAccounts().first());
}

AccountsComboBox::~AccountsComboBox()
{
}

void AccountsComboBox::setAccount(PsiAccount *_pa)
{
	pa = _pa;
	updateAccounts();
}

void AccountsComboBox::changeAccount()
{
	int i = currentItem();

	int n = 0;
	bool found = false;
	foreach(PsiAccount* p, psi->contactList()->enabledAccounts()) {
		if (!onlineOnly || p->loggedIn()) {
			if(i == n) {
				pa = p;
				found = true;
				break;
			}
			++n;
		}
	}
	if(!found)
		pa = 0;

	activated(pa);
}

void AccountsComboBox::updateAccounts()
{
	clear();
	int n = 0;
	bool found = false;
	PsiAccount *firstAccount = 0;
	foreach(PsiAccount* p, psi->contactList()->enabledAccounts()) {
		if (!onlineOnly || p->loggedIn()) {
			insertItem(p->nameWithJid());
			if(p == pa) {
				setCurrentItem(n);
				found = true;
			}
			if (!firstAccount)
				firstAccount = p;
			++n;
		}
	}
	if(!found) {
		// choose a different account
		pa = firstAccount;
		activated(pa);
	}
}

void AccountsComboBox::deleteMe()
{
	delete this;
}


