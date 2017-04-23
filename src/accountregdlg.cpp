/*
 * accountregdlg.cpp - dialogs for manipulating PsiAccounts
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QtCrypto>
#include <QMessageBox>
#include <QScrollArea>

#include "accountregdlg.h"
#include "proxy.h"
#include "serverlistquerier.h"
#include "miniclient.h"
#include "xmpp_tasks.h"
#include "psioptions.h"
#include "jidutil.h"
#include "textutil.h"
#include "xdata_widget.h"
#include "psicon.h"

using namespace XMPP;

AccountRegDlg::AccountRegDlg(PsiCon *psi, QWidget *parent) :
    QDialog(parent),
    psi(psi)
{
	ui_.setupUi(this);
	setModal(false);

	// TODO: If the domain is fixed, and the connection settings are fixed, skip first
	// step

	// Initialize settings
	ssl_ = UserAccount::SSL_Auto;
	port_ = 5222;

	// Server select button
	connect(ui_.le_server,SIGNAL(popup()),SLOT(selectServer()));
	serverlist_querier_ = new ServerListQuerier(this);
	connect(serverlist_querier_,SIGNAL(listReceived(const QStringList&)),SLOT(serverListReceived(const QStringList&)));
	connect(serverlist_querier_,SIGNAL(error(const QString&)),SLOT(serverListError(const QString&)));

	// Manual Host/Port
	ui_.le_host->setEnabled(false);
	ui_.lb_host->setEnabled(false);
	ui_.le_port->setEnabled(false);
	ui_.lb_port->setEnabled(false);
	connect(ui_.ck_host, SIGNAL(toggled(bool)), SLOT(hostToggled(bool)));

	// SSL
	ui_.cb_ssl->addItem(tr("Always"),UserAccount::SSL_Yes);
	ui_.cb_ssl->addItem(tr("When available"),UserAccount::SSL_Auto);
	ui_.cb_ssl->addItem(tr("Legacy SSL"), UserAccount::SSL_Legacy);
	ui_.cb_ssl->setCurrentIndex(ui_.cb_ssl->findData(ssl_));
	connect(ui_.cb_ssl, SIGNAL(activated(int)), SLOT(sslActivated(int)));

	// Cancel and next buttons
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_next, SIGNAL(clicked()), SLOT(next()));

	// Proxy
	proxy_chooser_ = ProxyManager::instance()->createProxyChooser(ui_.gb_connection);
	replaceWidget(ui_.lb_proxychooser, proxy_chooser_);
	proxy_chooser_->setCurrentItem(0);

	// Fields pane
	QVBoxLayout *fields_layout = new QVBoxLayout(ui_.page_fields);
	fields_layout->setMargin(0);
	fields_container_ = new QScrollArea(ui_.page_fields);
	fields_layout->addWidget(fields_container_);
	fields_container_->setWidgetResizable(true);
	fields_layout->addStretch(20);
	fields_ = NULL;

	ui_.le_port->setText(QString::number(port_));
	ui_.le_host->setFocus();

	client_ = new MiniClient;
	connect(client_, SIGNAL(handshaken()), SLOT(client_handshaken()));
	connect(client_, SIGNAL(error()), SLOT(client_error()));

	if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
		ui_.gb_server->hide();
	}
}

AccountRegDlg::~AccountRegDlg()
{
	delete client_;
}

void AccountRegDlg::done(int r)
{
	if(ui_.busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel the registration?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	client_->close();
	QDialog::done(r);
}

void AccountRegDlg::sslActivated(int i)
{
	if ((ui_.cb_ssl->itemData(i) == UserAccount::SSL_Yes || ui_.cb_ssl->itemData(i) == UserAccount::SSL_Legacy) && !checkSSL()) {
		ui_.cb_ssl->setCurrentIndex(ui_.cb_ssl->findData(UserAccount::SSL_Auto));
	}
	else if (ui_.cb_ssl->itemData(i) == UserAccount::SSL_Legacy && !ui_.ck_host->isChecked()) {
		QMessageBox::critical(this, tr("Error"), tr("Legacy SSL is only available in combination with manual host/port."));
		ui_.cb_ssl->setCurrentIndex(ui_.cb_ssl->findData(UserAccount::SSL_Auto));
	}
}


bool AccountRegDlg::checkSSL()
{
	if(!QCA::isSupported("tls")) {
		QMessageBox::information(this, tr("SSL error"), tr("Cannot enable SSL/TLS. QCA2 Plugin not found."));
		return false;
	}
	return true;
}

void AccountRegDlg::hostToggled(bool on)
{
	ui_.le_host->setEnabled(on);
	ui_.le_port->setEnabled(on);
	ui_.lb_host->setEnabled(on);
	ui_.lb_port->setEnabled(on);
	if (!on && ui_.cb_ssl->currentIndex() == ui_.cb_ssl->findData(UserAccount::SSL_Legacy)) {
		ui_.cb_ssl->setCurrentIndex(ui_.cb_ssl->findData(UserAccount::SSL_Auto));
	}
}

void AccountRegDlg::selectServer()
{
	if (ui_.le_server->count() == 0) {
		ui_.busy->start();
		block();
		serverlist_querier_->getList();
	}
}

void AccountRegDlg::serverListReceived(const QStringList& list)
{
	ui_.busy->stop();
	unblock();
	ui_.le_server->clear();
	ui_.le_server->addItems(list);
	ui_.le_server->showPopup();
}

void AccountRegDlg::serverListError(const QString& e)
{
	ui_.busy->stop();
	unblock();
	QString error = tr("There was an error retrieving the server list");
	if (!e.isEmpty()) {
		error += ".\n" + tr("Reason: ") + e;
	}
	qWarning("%s", qPrintable(error));
	//QMessageBox::critical(this, tr("Error"), error);
	ui_.le_server->setFocus();
}

void AccountRegDlg::next()
{
	if (ui_.sw_register->currentWidget() == ui_.page_server) {

		// Update settings
		server_ = JIDUtil::accountFromString(ui_.le_server->currentText().trimmed());
		ssl_ =  (UserAccount::SSLFlag) ui_.cb_ssl->itemData(ui_.cb_ssl->currentIndex()).toInt();
		opt_host_ = ui_.ck_host->isChecked();
		host_ = ui_.le_host->text();
		port_ = ui_.le_port->text().toInt();
		proxy_ = proxy_chooser_->currentItem();

		// Sanity check
		if (server_.isNull() || !server_.node().isEmpty() || !server_.resource().isEmpty()) {
			QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid server name"));
			return;
		}

		// Connect to the server
		ui_.busy->start();
		block();
		client_->connectToServer(server_, false, ssl_ == UserAccount::SSL_Legacy, ssl_ == UserAccount::SSL_Yes, opt_host_ ? host_ : QString(), port_, proxy_);
	}
	else if (ui_.sw_register->currentWidget() == ui_.page_fields) {
		// Initialize the form
		XMPP::XData fields;
		fields.setFields(fields_->fields());

		// Determine the username and password
		foreach(XMPP::XData::Field field, fields.fields()) {
			if (field.var() == "username" && !field.value().isEmpty()) {
				jid_ = Jid(field.value().at(0), server_.bare(), "");
			}
			else if (field.var() == "password" && !field.value().isEmpty()) {
				pass_ = field.value().at(0);
			}
		}

		// Register
		ui_.busy->start();
		block();
		JT_Register *reg = new JT_Register(client_->client()->rootTask());
		connect(reg, SIGNAL(finished()), SLOT(setFields_finished()));
		if (isOld_) {
			Form form = convertFromXData(fields);
			form.setJid(server_);
			reg->setForm(form);
		}
		else {
			reg->setForm(server_,fields);
		}
		reg->go(true);
	}
}

void AccountRegDlg::client_handshaken()
{
	// try to register an account
	JT_Register *reg = new JT_Register(client_->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(getFields_finished()));
	reg->getForm(server_);
	reg->go(true);
}

void AccountRegDlg::client_error()
{
	ui_.busy->stop();
	unblock();
	if (ui_.sw_register->currentWidget() == ui_.page_fields) {
		// Start over
		delete fields_;
		fields_ = NULL;
		ui_.sw_register->setCurrentWidget(ui_.page_server);
	}
}

void AccountRegDlg::getFields_finished()
{
	JT_Register *reg = (JT_Register *)sender();
	ui_.busy->stop();
	if (reg->success()) {
		unblock();
		fields_ =  new XDataWidget(psi, ui_.page_fields, client_->client(), reg->form().jid());
		XData xdata;
		if (reg->hasXData()) {
			isOld_ = false;
			xdata = reg->xdata();
		}
		else {
			isOld_ = true;
			xdata = convertToXData(reg->form());
		}
		if (xdata.instructions().isEmpty())
			xdata.setInstructions(tr("Please provide the following information:"));
		xdata.setInstructions(TextUtil::linkify(xdata.instructions()));
		fields_->setForm(xdata);
		fields_container_->setWidget(fields_);
		fields_container_->updateGeometry();
		ui_.sw_register->setCurrentWidget(ui_.page_fields);
	}
	else {
		QMessageBox::critical(this, tr("Error"), tr("This server does not support registration"));
		unblock();
	}
}

void AccountRegDlg::setFields_finished()
{
	JT_Register *reg = (JT_Register *)sender();
	ui_.busy->stop();
	if (reg->success()) {
		QMessageBox::information(this, tr("Success"), QString(tr("You have successfully registered your account with XMPP address '%1'")).arg(jid_.bare()));
		tlsOverrideCert_ = client_->tlsOverrideCert;
		tlsOverrideDomain_ = client_->tlsOverrideDomain;
		client_->close();
		accept();
	}
	else {
		unblock();
		QMessageBox::critical(this, tr("Error"), QString(tr("There was an error registering the account.\nReason: %1")).arg(reg->statusString()));
	}
}

XMPP::XData AccountRegDlg::convertToXData(const XMPP::Form& form)
{
	// Convert the fields
	XData::FieldList fields;
	foreach(FormField f, form) {
		XData::Field field;
		field.setLabel(f.fieldName());
		field.setVar(f.realName());
		field.setRequired(true);
		if (f.isSecret()) {
			field.setType(XData::Field::Field_TextPrivate);
		}
		else {
			field.setType(XData::Field::Field_TextSingle);
		}
		fields.push_back(field);
	}

	// Create the form
	XData xdata;
	xdata.setInstructions(form.instructions());
	xdata.setFields(fields);
	return xdata;
}

XMPP::Form AccountRegDlg::convertFromXData(const XMPP::XData& xdata)
{
	Form form;
	foreach(XMPP::XData::Field field, xdata.fields()) {
		if (!field.value().isEmpty()) {
			FormField f;
			f.setType(field.var());
			f.setValue(field.value().at(0));
			form.push_back(f);
		}
	}
	return form;
}

void AccountRegDlg::block()
{
	if (ui_.sw_register->currentWidget() == ui_.page_server) {
		ui_.gb_server->setEnabled(false);
		ui_.gb_connection->setEnabled(false);
		ui_.pb_next->setEnabled(false);
	}
	else if (ui_.sw_register->currentWidget() == ui_.page_fields) {
		if (fields_)
			fields_->setEnabled(false);
	}
}

void AccountRegDlg::unblock()
{
	if (ui_.sw_register->currentWidget() == ui_.page_server) {
		ui_.gb_server->setEnabled(true);
		ui_.gb_connection->setEnabled(true);
		ui_.pb_next->setEnabled(true);
	}
	else if (ui_.sw_register->currentWidget() == ui_.page_fields) {
		ui_.pb_next->setEnabled(true);
		if (fields_)
			fields_->setEnabled(true);
	}
}



