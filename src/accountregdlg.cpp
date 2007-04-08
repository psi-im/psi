/*
 * accountregdlg.cpp - dialogs for manipulating PsiAccounts
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

#include <QtCrypto>
#include <QMessageBox>

#include "accountregdlg.h"
#include "proxy.h"
#include "miniclient.h"
#include "common.h" // CAP()
#include "xmpp_tasks.h"
#include "psioptions.h"
#include "jidutil.h"

using namespace XMPP;


AccountRegDlg::AccountRegDlg(ProxyManager *_proxyman, QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(false);
  	setWindowTitle(CAP(caption()));
	
	connect(ck_host, SIGNAL(toggled(bool)), ck_ssl, SLOT(setEnabled(bool)));
	ck_legacy_ssl_probe->setEnabled(true);
	ck_ssl->setEnabled(false);
	ck_legacy_ssl_probe->setChecked(true);

	le_host->setEnabled(false);
	lb_host->setEnabled(false);
	le_port->setEnabled(false);
	lb_port->setEnabled(false);

	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_reg, SIGNAL(clicked()), SLOT(reg()));
	connect(ck_ssl, SIGNAL(toggled(bool)), SLOT(sslToggled(bool)));
	connect(ck_host, SIGNAL(toggled(bool)), SLOT(hostToggled(bool)));

	proxyman = _proxyman;
	pc = proxyman->createProxyChooser(gb_proxy);
	replaceWidget(lb_proxychooser, pc);
	pc->fixTabbing(le_confirm, ck_ssl);
	pc->setCurrentItem(0);

	le_port->setText("5222");
	le_host->setFocus();

	client = new MiniClient;
	connect(client, SIGNAL(handshaken()), SLOT(client_handshaken()));
	connect(client, SIGNAL(error()), SLOT(client_error()));
	
	if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
		lb_example->hide();
		lb_jid->setText(tr("Username:"));
	}
}

AccountRegDlg::~AccountRegDlg()
{
	delete client;
}

/*void AccountRegDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void AccountRegDlg::done(int r)
{
	if(busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel the registration?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	QDialog::done(r);
}

void AccountRegDlg::sslToggled(bool on)
{
	if (on && !checkSSL()) {
		ck_ssl->setChecked(false);
		return;
	}

	le_port->setText(on ? "5223": "5222");
}

bool AccountRegDlg::checkSSL()
{
	if(!QCA::isSupported("tls")) {
		QMessageBox::information(this, tr("SSL error"), tr("Cannot enable SSL/TLS.  Plugin not found."));
		return false;
	}
	return true;
}

void AccountRegDlg::hostToggled(bool on)
{
	le_host->setEnabled(on);
	le_port->setEnabled(on);
	lb_host->setEnabled(on);
	lb_port->setEnabled(on);
	ck_legacy_ssl_probe->setEnabled(!on);
}

void AccountRegDlg::reg()
{
	// sanity check
	Jid j(JIDUtil::accountFromString(le_jid->text().trimmed()));
	if ( j.user().isEmpty() || j.host().isEmpty() ) {
		if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
			QMessageBox::information(this, tr("Error"), tr("<i>Username</i> is invalid."));
		}
		else {
			QMessageBox::information(this, tr("Error"), tr("<i>Jabber ID</i> must be specified in the format <i>user@host</i>."));
		}
		return;
	}

	if(le_pass->text().isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields properly before you can register."));
		return;
	}

	if(le_pass->text() != le_confirm->text()) {
		QMessageBox::information(this, tr("Error"), tr("Password and confirmation do not match.  Please enter them again."));
		le_pass->setText("");
		le_confirm->setText("");
		le_pass->setFocus();
		return;
	}

	busy->start();
	block();

	jid = JIDUtil::accountFromString(le_jid->text().trimmed()).bare();
	ssl = ck_ssl->isChecked();
	pass = le_pass->text();
	opt_host = ck_host->isChecked();
	sp_host = le_host->text();
	sp_port = le_port->text().toInt();
	legacy_ssl_probe = (opt_host ? false : ck_legacy_ssl_probe->isChecked());

	client->connectToServer(jid, legacy_ssl_probe, ssl, opt_host ? sp_host : QString(), sp_port, proxyman, pc->currentItem(), 0);
}

void AccountRegDlg::client_handshaken()
{
	// try to register an account
	JT_Register *reg = new JT_Register(client->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(reg_finished()));
	reg->reg(jid.user(), pass);
	reg->go(true);
}

void AccountRegDlg::client_error()
{
	busy->stop();
	unblock();
}

void AccountRegDlg::reg_finished()
{
	JT_Register *reg = (JT_Register *)sender();

	client->close();
	busy->stop();

	if(reg->success()) {
		QMessageBox::information(this, tr("Success"), tr("The account was registered successfully."));
		proxy = pc->currentItem();
		accept();
		return;
	}
	else if(reg->statusCode() != Task::ErrDisc) {
		unblock();
		QMessageBox::critical(this, tr("Error"), QString(tr("There was an error registering the account.\nReason: %1")).arg(reg->statusString()));
	}
}

void AccountRegDlg::block()
{
	gb_account->setEnabled(false);
	gb_proxy->setEnabled(false);
	gb_advanced->setEnabled(false);
	pb_reg->setEnabled(false);
}

void AccountRegDlg::unblock()
{
	gb_account->setEnabled(true);
	gb_proxy->setEnabled(true);
	gb_advanced->setEnabled(true);
	pb_reg->setEnabled(true);
	le_jid->setFocus();
}



