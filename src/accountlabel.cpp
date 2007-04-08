/*
 * accountlabel.cpp - simple label to display account name currently in use
 * Copyright (C) 2006  Michail Pishchagin
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

AccountLabel::AccountLabel(PsiAccount* _pa, QWidget* par, bool smode)
	: QLabel(par)
{
	pa = _pa;
	simpleMode = smode;
	setFrameStyle( QFrame::Panel | QFrame::Sunken );

	updateName();
	connect(pa, SIGNAL(updatedAccount()), this, SLOT(updateName()));
	connect(pa, SIGNAL(destroyed()), this, SLOT(deleteMe()));
}

AccountLabel::~AccountLabel()
{
}

void AccountLabel::updateName()
{
	setText(simpleMode ? pa->name() : pa->nameWithJid());
}

void AccountLabel::deleteMe()
{
	delete this;
}
