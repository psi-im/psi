/*
 * changepwdlg.cpp - dialog for changing password
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

#include "changepwdlg.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include "profiles.h"
#include "psiaccount.h"
#include "busywidget.h"
#include "xmpp_tasks.h"
#include "accountmodifydlg.h"
#include "iconwidget.h"

using namespace XMPP;

ChangePasswordDlg::ChangePasswordDlg(PsiAccount *_pa, QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setupUi(this);
	setModal(false);
  	pa = _pa;
	pa->dialogRegister(this);

	connect(pa, SIGNAL(disconnected()), SLOT(disc()));

	setWindowTitle(CAP(caption()));

	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_apply, SIGNAL(clicked()), SLOT(apply()));
}

ChangePasswordDlg::~ChangePasswordDlg()
{
	pa->dialogUnregister(this);
}

/*void ChangePasswordDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void ChangePasswordDlg::done(int r)
{
	if(busy->isActive())
		return;
	QDialog::done(r);
}

void ChangePasswordDlg::apply()
{
	// sanity check
	if(le_pwcur->text().isEmpty() || le_pwnew->text().isEmpty() || le_pwver->text().isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields properly before you can proceed."));
		return;
	}

	if(le_pwcur->text() != pa->userAccount().pass) {
		QMessageBox::information(this, tr("Error"), tr("You entered your old password incorrectly.  Try again."));
		le_pwcur->setText("");
		le_pwcur->setFocus();
		return;
	}

	if(le_pwnew->text() != le_pwver->text()) {
		QMessageBox::information(this, tr("Error"), tr("New password and confirmation do not match.  Please enter them again."));
		le_pwnew->setText("");
		le_pwver->setText("");
		le_pwnew->setFocus();
		return;
	}

	busy->start();
	blockWidgets();

	JT_Register *reg = new JT_Register(pa->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(finished()));
	Jid j = pa->userAccount().jid;
	reg->reg(j.node(), le_pwnew->text());
	reg->go(true);
}

void ChangePasswordDlg::finished()
{
	busy->stop();

	JT_Register *reg = (JT_Register *)sender();
	QString err = reg->statusString();
	int code = reg->statusCode();
	bool ok = reg->success();
	if(ok) {
		UserAccount acc = pa->userAccount();
		acc.pass = le_pwnew->text();
		pa->setUserAccount(acc);
		AccountModifyDlg *amd = pa->findDialog<AccountModifyDlg*>();
		if(amd)
			amd->setPassword(acc.pass);

		QMessageBox::information(this, tr("Success"), tr("Successfully changed password."));
		close();
	}
	else {
		// ignore task disconnect error
		if(code == Task::ErrDisc)
			return;

		QMessageBox::critical(this, tr("Error"), QString(tr("There was an error when trying to set the password.\nReason: %1")).arg(QString(err).replace('\n', "<br>")));
		restoreWidgets();
	}
}

void ChangePasswordDlg::disc()
{
	busy->stop();
	close();
}

void ChangePasswordDlg::blockWidgets()
{
	setWidgetsEnabled(false);
}

void ChangePasswordDlg::restoreWidgets()
{
	setWidgetsEnabled(true);
}

void ChangePasswordDlg::setWidgetsEnabled(bool enabled)
{
	lb_pwcur->setEnabled(enabled);
	lb_pwnew->setEnabled(enabled);
	lb_pwver->setEnabled(enabled);
	le_pwcur->setEnabled(enabled);
	le_pwnew->setEnabled(enabled);
	le_pwver->setEnabled(enabled);
	pb_close->setEnabled(enabled);
	pb_apply->setEnabled(enabled);
}
