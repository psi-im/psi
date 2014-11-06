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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "changepwdlg.h"

#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QFormLayout>

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
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	ui_.setupUi(this);

	pa = _pa;
	pa->dialogRegister(this);

	connect(pa, SIGNAL(disconnected()), SLOT(disc()));

	setWindowTitle(CAP(windowTitle()));

	connect(ui_.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(close()));
	connect(ui_.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(apply()));
	adjustSize();
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
	if(ui_.busy->isActive())
		return;
	QDialog::done(r);
}

void ChangePasswordDlg::apply()
{
	// sanity check
	if(ui_.le_pwcur->text().isEmpty() || ui_.le_pwnew->text().isEmpty() || ui_.le_pwver->text().isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields properly before you can proceed."));
		return;
	}

	if(ui_.le_pwcur->text() != pa->userAccount().pass) {
		QMessageBox::information(this, tr("Error"), tr("You entered your old password incorrectly.  Try again."));
		ui_.le_pwcur->setText("");
		ui_.le_pwcur->setFocus();
		return;
	}

	if(ui_.le_pwnew->text() != ui_.le_pwver->text()) {
		QMessageBox::information(this, tr("Error"), tr("New password and confirmation do not match.  Please enter them again."));
		ui_.le_pwnew->setText("");
		ui_.le_pwver->setText("");
		ui_.le_pwnew->setFocus();
		return;
	}

	ui_.busy->start();
	blockWidgets();

	JT_Register *reg = new JT_Register(pa->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(finished()));
	Jid j = pa->userAccount().jid;
	reg->reg(j.node(), ui_.le_pwnew->text());
	reg->go(true);
}

void ChangePasswordDlg::finished()
{
	ui_.busy->stop();

	JT_Register *reg = (JT_Register *)sender();
	QString err = reg->statusString();
	int code = reg->statusCode();
	bool ok = reg->success();
	if(ok) {
		UserAccount acc = pa->userAccount();
		acc.pass = ui_.le_pwnew->text();
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
	ui_.busy->stop();
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
	foreach(QWidget* w, findChildren<QWidget*>()) {
		w->setEnabled(enabled);
	}
}
