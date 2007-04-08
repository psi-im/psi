/*
 * accountlabel.cpp - simple label to display account name currently in use
 * Copyright (C) 2006-2007  Michail Pishchagin
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

#include "accountlabel.h"
#include "psiaccount.h"

AccountLabel::AccountLabel(QWidget* parent)
	: QLabel(parent)
	, showJid_(true)
{
	setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

AccountLabel::~AccountLabel()
{
}

PsiAccount* AccountLabel::account() const
{
	return account_;
}

bool AccountLabel::showJid() const
{
	return showJid_;
}

void AccountLabel::setAccount(PsiAccount* account)
{
	account_ = account;
	if (account) {
		connect(account, SIGNAL(updatedAccount()), SLOT(updateName()));
		connect(account, SIGNAL(destroyed()), SLOT(deleteLater()));
	}
	updateName();
}

void AccountLabel::setShowJid(bool showJid)
{
	showJid_ = showJid;
	updateName();
}

void AccountLabel::updateName()
{
	QString text = "...";
	if (!account_.isNull()) {
		if (showJid_)
			text = account_->nameWithJid();
		else
			text = account_->name();
	}
	setText(text);
}
