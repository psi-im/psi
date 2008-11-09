/*
 * Copyright (C) 2008 Martin Hostettler
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Query dialog for password on login.

#ifndef ACCOUNTLOGINPASSWORD_H
#define ACCOUNTLOGINPASSWORD_H

#include <QLineEdit>
#include <QPointer>
#include <QDialog>

#include "xmpp_status.h"

using namespace XMPP;

class PsiAccount;
class PsiCon;

class AccountLoginPassword : public QDialog {
	Q_OBJECT
public:
	AccountLoginPassword(PsiAccount *account);
	
	
private slots:
	void accept();
	void reject();
	
private:
	QPointer<PsiAccount> account_;
	PsiCon *psi_;
	QLineEdit le_;
};


#endif
