/*
 * accountscombobox.h
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

#ifndef ACCOUNTSCOMBOBOX_H
#define ACCOUNTSCOMBOBOX_H

#include <QComboBox>

class PsiCon;
class PsiAccount;

class AccountsComboBox : public QComboBox
{
	Q_OBJECT
public:
	AccountsComboBox(QWidget* parent);
	~AccountsComboBox();

	PsiAccount* account() const;
	void setAccount(PsiAccount* account);

	PsiCon* controller() const;
	void setController(PsiCon* controller);

	bool onlineOnly() const;
	void setOnlineOnly(bool onlineOnly);

signals:
	void activated(PsiAccount* account);

private slots:
	void changeAccount();
	void updateAccounts();

private:
	PsiCon* controller_;
	PsiAccount* account_;
	bool onlineOnly_;

	QList<PsiAccount*> accounts() const;
};

#endif
