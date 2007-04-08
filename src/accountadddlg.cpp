/*
 * accountadddlg.cpp - dialogs for manipulating PsiAccounts
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

#include "accountadddlg.h"
#include "psicon.h"
#include "psioptions.h"
#include "psiaccount.h"
#include "accountregdlg.h"
#include "psicontactlist.h"

AccountAddDlg::AccountAddDlg(PsiCon *_psi, QWidget *parent)
:QDialog(parent)
{
  	setupUi(this);
	setModal(false);
	psi = _psi;
	psi->dialogRegister(this);

	setWindowTitle(CAP(caption()));

	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_add, SIGNAL(clicked()), SLOT(add()));
	connect(le_name, SIGNAL(textChanged(const QString &)), SLOT(setAddButton(const QString &)));

	ck_reg->setWhatsThis(
		tr("Check this option if you don't yet have a Jabber account "
		"and you want to register one.  Note that this will only work "
		"on servers that allow anonymous registration."));

	QString aname = createNewAccountName(tr("Default"));

	if (PsiOptions::instance()->getOption("options.ui.account.single").toBool()) {
		le_name->setText("account");
		lb_name->hide();
		le_name->hide();
	}
	else {
		le_name->setText(aname);
		le_name->setFocus();
	}
}

AccountAddDlg::~AccountAddDlg()
{
	psi->dialogUnregister(this);
}

QString AccountAddDlg::createNewAccountName(QString def)
{
	QString aname = def;
	int n = 0;
	while(1) {
		bool taken = false;
		foreach(PsiAccount* pa, psi->contactList()->accounts()) {
			if(aname == pa->name()) {
				taken = true;
				break;
			}
		}

		if(!taken)
			break;
		aname = def + '_' + QString::number(++n);
	}

	return aname;
}

void AccountAddDlg::add()
{
	QString aname = createNewAccountName(le_name->text());
	le_name->setText( aname );

	if(ck_reg->isChecked()) {
		AccountRegDlg *w = new AccountRegDlg(psi->proxy(), this);
		int n = w->exec();
		if(n != QDialog::Accepted) {
			delete w;
			return;
		}

		Jid jid = w->jid;
		QString pass = w->pass;
		bool opt_host = w->opt_host;
		QString host = w->sp_host;
		int port = w->sp_port;
		bool ssl = w->ssl;
		int proxy = w->proxy;

		delete w;

		psi->createAccount(le_name->text(), jid, pass, opt_host, host, port, ssl, proxy);
	}
	else {
		psi->createAccount(le_name->text());
	}

	close();
}

void AccountAddDlg::setAddButton(const QString &s)
{
	pb_add->setEnabled(!s.isEmpty());
}



