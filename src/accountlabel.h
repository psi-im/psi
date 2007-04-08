/*
 * accountlabel.h - simple label to display account name currently in use
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

#ifndef ACCOUNTLABEL_H
#define ACCOUNTLABEL_H

#include <QLabel>

class PsiAccount;

class AccountLabel : public QLabel
{
	Q_OBJECT
public:
	AccountLabel(PsiAccount*, QWidget* parent, bool simpleMode = false);
	~AccountLabel();

private slots:
	void updateName();
	void deleteMe();

private:
	PsiAccount *pa;
	bool simpleMode;
};

#endif
