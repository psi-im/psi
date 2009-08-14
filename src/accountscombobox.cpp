/*
 * accountscombobox.cpp
 * Copyright (C) 2001-2008  Justin Karneges, Michail Pishchagin
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

AccountsComboBox::AccountsComboBox(QWidget* parent)
	: QComboBox(parent)
	, controller_(0)
	, account_(0)
	, onlineOnly_(false)
{
	setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	connect(this, SIGNAL(activated(int)), this, SLOT(changeAccount()));
}

AccountsComboBox::~AccountsComboBox()
{
}

PsiAccount* AccountsComboBox::account() const
{
	return account_;
}

void AccountsComboBox::setAccount(PsiAccount* account)
{
	account_ = account;
	updateAccounts();
}

PsiCon* AccountsComboBox::controller() const
{
	return controller_;
}

void AccountsComboBox::setController(PsiCon* controller)
{
	if (controller_) {
		disconnect(controller_, SIGNAL(accountCountChanged()), this, SLOT(updateAccounts()));
		disconnect(controller_, SIGNAL(accountActivityChanged()), this, SLOT(updateAccounts()));
	}

	controller_ = controller;

	if (controller_) {
		connect(controller_, SIGNAL(accountCountChanged()), this, SLOT(updateAccounts()));
		connect(controller_, SIGNAL(accountActivityChanged()), this, SLOT(updateAccounts()));
	}

	if (controller_->contactList()->haveEnabledAccounts()) {
		setAccount(controller_->contactList()->enabledAccounts().first());
	}

	updateAccounts();
}

bool AccountsComboBox::onlineOnly() const
{
	return onlineOnly_;
}

void AccountsComboBox::setOnlineOnly(bool onlineOnly)
{
	onlineOnly_ = onlineOnly;
	updateAccounts();
}

void AccountsComboBox::changeAccount()
{
	account_ = 0;
	if (currentIndex() >= 0 && currentIndex() < accounts().count())
		account_ = accounts().at(currentIndex());
	emit activated(account_);
}

void AccountsComboBox::updateAccounts()
{
	clear();

	foreach(PsiAccount* account, accounts())
		addItem(account->nameWithJid());

	if (accounts().indexOf(account_) == -1) {
		account_ = accounts().isEmpty() ? 0 : accounts().first();
		emit activated(account_);
	}
	setCurrentIndex(accounts().indexOf(account_));
}

QList<PsiAccount*> AccountsComboBox::accounts() const
{
	QList<PsiAccount*> result;
	if (controller_) {
		foreach(PsiAccount* account, controller_->contactList()->enabledAccounts())
			if (!onlineOnly_ || account->isAvailable())
				result << account;
	}

	return result;
}
