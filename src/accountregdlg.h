/*
 * accountregdlg.h - dialogs for manipulating PsiAccounts
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

#ifndef ACCOUNTREGDLG_H
#define ACCOUNTREGDLG_H

#include <QDialog>
#include <QString>

#include "xmpp.h"
#include "ui_accountreg.h"

class ProxyManager;
class ProxyChooser;
class QWidget;
class MiniClient;

class AccountRegDlg : public QDialog, public Ui::AccountReg
{
	Q_OBJECT
public:
	AccountRegDlg(ProxyManager *, QWidget *parent=0);
	~AccountRegDlg();

	XMPP::Jid jid;
	QString sp_host, pass;
	int sp_port;
	bool ssl, opt_host, legacy_ssl_probe;
	int proxy;

protected:
	bool checkSSL();
	// reimplemented
	//void closeEvent(QCloseEvent *);

public slots:
	void done(int);

private slots:
	void legacySSLToggled(bool);
	void sslToggled(bool);
	void hostToggled(bool);
	void reg();

	void client_handshaken();
	void client_error();

	void reg_finished();

	void block();
	void unblock();

private:
	ProxyManager *proxyman;
	ProxyChooser *pc;
	MiniClient *client;
};

#endif
