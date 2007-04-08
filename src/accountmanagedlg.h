/*
 * accountmanagedlg.h - dialogs for manipulating PsiAccounts
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

#ifndef ACCOUNTMANAGEDLG_H
#define ACCOUNTMANAGEDLG_H


#include "ui_accountmanage.h"

namespace XMPP
{
	class Jid;
	class Client;
}

class PsiCon;
class PsiAccount;
class Q3ListViewItem;

class AccountManageDlg : public QDialog, public Ui::AccountManage
{
	Q_OBJECT
public:
	AccountManageDlg(PsiCon *);
	~AccountManageDlg();

private slots:
	void qlv_selectionChanged(Q3ListViewItem *);
	void add();
	void modify();
	void modify(Q3ListViewItem *);
	void remove();
	void accountAdded(PsiAccount *);
	void accountUpdated(PsiAccount *);
	void accountRemoved(PsiAccount *);

private:
	PsiCon *psi;
};

#endif
