/*
 * accountregdlg.h
 * Copyright (C) 2001, 2002, 2006  Justin Karneges, Remko Troncon
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

#include "profiles.h"
#include "xmpp_jid.h"
#include "ui_accountreg.h"

class ProxyManager;
class ProxyChooser;
class QWidget;
class QScrollArea;
class QStringList;
class MiniClient;
class XDataWidget;
class ServerListQuerier;
class QByteArray;
namespace XMPP {
	class Form;
	class XData;
}

class AccountRegDlg : public QDialog
{
	Q_OBJECT
public:
	AccountRegDlg(ProxyManager*, QWidget *parent=0);
	~AccountRegDlg();

	const XMPP::Jid& jid() const { return jid_; }
	const QString& pass() const { return pass_; }
	bool useHost() const { return opt_host_; }
	const QString& host() const { return host_; }
	int port() const { return port_; }
	bool legacySSLProbe() { return legacy_ssl_probe_; }
	UserAccount::SSLFlag ssl() const { return ssl_; }
	QString proxy() const { return proxy_; }
	QString tlsOverrideDomain() { return tlsOverrideDomain_; };
	QByteArray tlsOverrideCert() { return tlsOverrideCert_; };

public slots:
	void done(int);

protected:
	static XMPP::XData convertToXData(const XMPP::Form&);
	static XMPP::Form convertFromXData(const XMPP::XData&);

	bool checkSSL();
	void block();
	void unblock();

protected slots:
	void hostToggled(bool);
	void sslActivated(int);
	void next();

	void selectServer();
	void serverListReceived(const QStringList&);
	void serverListError(const QString&);

	void client_handshaken();
	void client_error();

	void getFields_finished();
	void setFields_finished();

private:
	Ui::AccountReg ui_;
	QScrollArea* fields_container_;
	XDataWidget* fields_;
	ProxyManager *proxy_manager_;
	ProxyChooser *proxy_chooser_;
	ServerListQuerier *serverlist_querier_;
	MiniClient *client_;
	bool isOld_;

	// Account settings
	XMPP::Jid jid_, server_;
	UserAccount::SSLFlag ssl_;
	bool opt_host_, legacy_ssl_probe_;
	QString host_;
	int port_;
	QString pass_;
	QString proxy_;
	QString tlsOverrideDomain_;
	QByteArray tlsOverrideCert_;
};

#endif
