/*
 * accountmodifydlg.cpp
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

#include <QInputDialog>
#include <QMessageBox>

#include "accountmodifydlg.h"
#include "pgputil.h"
#include "psiaccount.h"
#include "iconset.h"
#include "psioptions.h"
#include "jidutil.h"
#include "profiles.h"
#include "psicon.h"
#include "proxy.h"
#include "privacymanager.h"
#include "privacydlg.h"
#include "pgpkeydlg.h"
#include "psicontactlist.h"

AccountModifyDlg::AccountModifyDlg(PsiAccount *_pa, QWidget *parent)
:QDialog(parent)
{
  	setupUi(this);
	setModal(false);
	pa = _pa;
	//connect(pa->psi(), SIGNAL(pgpToggled(bool)), SLOT(pgpToggled(bool)));
	pa->dialogRegister(this);

	setWindowTitle(CAP(caption()));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/account").icon());
#endif

	const UserAccount &acc = pa->userAccount();
	
	le_pass->setEnabled(true);
	le_host->setEnabled(false);
	lb_host->setEnabled(false);
	le_port->setEnabled(false);
	lb_port->setEnabled(false);

	ck_ssl->setEnabled(false);
#ifdef __GNUC__
#warning "Temporarily removing security level settings"
#endif
	ck_req_mutual->hide();
	cb_security_level->hide();
	lb_security_level->hide();

	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	connect(ck_host, SIGNAL(toggled(bool)), SLOT(hostToggled(bool)));
	connect(pb_key, SIGNAL(clicked()), SLOT(chooseKey()));
	connect(pb_keyclear, SIGNAL(clicked()), SLOT(clearKey()));
	connect(pb_save, SIGNAL(clicked()), SLOT(save()));
	connect(ck_automatic_resource, SIGNAL(toggled(bool)), le_resource, SLOT(setDisabled(bool)));
	connect(ck_automatic_resource, SIGNAL(toggled(bool)), lb_resource, SLOT(setDisabled(bool)));

	gb_pgp->setEnabled(false);

	connect(pb_vcard, SIGNAL(clicked()), SLOT(detailsVCard()));
	connect(pb_changepw, SIGNAL(clicked()), SLOT(detailsChangePW()));

	// Hide the name if necessary
	le_name->setText(acc.name);
	le_jid->setText(JIDUtil::accountToString(acc.jid,false));

	ck_ssl->setChecked(acc.opt_ssl);
	connect(ck_ssl, SIGNAL(toggled(bool)), SLOT(sslToggled(bool)));
	
	if (acc.opt_pass)
		le_pass->setText(acc.pass);

	ck_host->setChecked(acc.opt_host);
	le_host->setText(acc.host);
	le_port->setText(QString::number(acc.port));
	ck_req_mutual->setChecked(acc.req_mutual_auth);
	ck_legacy_ssl_probe->setChecked(acc.legacy_ssl_probe);

	ck_automatic_resource->setChecked(acc.opt_automatic_resource);
	le_resource->setText(acc.resource);
	le_priority->setText(QString::number(acc.priority));

	connect(ck_authzid,SIGNAL(toggled(bool)), le_authzid, SLOT(setEnabled(bool)));
	ck_authzid->setChecked(acc.useAuthzid);
	le_authzid->setText(acc.authzid);
#ifdef __GNUC__
#warning "Temporarily removing authzid (not fully implemented yet)"
#endif
	ck_authzid->hide();
	le_authzid->hide();
		
	ck_plain->setChecked(acc.opt_plain);
	ck_compress->setChecked(acc.opt_compress);
	ck_auto->setChecked(acc.opt_auto);
	ck_reconn->setChecked(acc.opt_reconn);
	ck_log->setChecked(acc.opt_log);
	ck_keepAlive->setChecked(acc.opt_keepAlive);
	ck_ignoreSSLWarnings->setChecked(acc.opt_ignoreSSLWarnings);
	le_dtProxy->setText(acc.dtProxy.full());

	key = acc.pgpSecretKey;
	updateUserID();
	if(PGPUtil::pgpAvailable()) {
		gb_pgp->setEnabled(true);
	}

	pc = pa->psi()->proxy()->createProxyChooser(gb_proxy);
	replaceWidget(lb_proxychooser, pc);
	pc->setCurrentItem(acc.proxy_index);
	
	// Security level
	cb_security_level->addItem(tr("None"),QCA::SL_None);
	cb_security_level->addItem(tr("Integrity"),QCA::SL_Integrity);
	cb_security_level->addItem(tr("Baseline"),QCA::SL_Baseline);
	cb_security_level->addItem(tr("High"),QCA::SL_High);
	cb_security_level->addItem(tr("Highest"),QCA::SL_Highest);
	cb_security_level->setCurrentIndex(cb_security_level->findData(acc.security_level));

	// Name
	if(le_name->text().isEmpty())
		le_name->setFocus();
	else if(le_jid->text().isEmpty())
		le_jid->setFocus();
	else
		pb_save->setFocus();

	// Privacy
	privacyInitialized = false;
	lb_customPrivacy->hide();
	privacyBlockedModel.setSourceModel(&privacyModel);
	lv_blocked->setModel(&privacyBlockedModel);
	connect(pa->privacyManager(),SIGNAL(defaultListAvailable(const PrivacyList&)),SLOT(updateBlockedContacts(const PrivacyList&)));
	connect(pa->privacyManager(),SIGNAL(defaultListError()),SLOT(getDefaultList_error()));
	connect(pa->privacyManager(),SIGNAL(changeList_error()),SLOT(changeList_error()));
	connect(pa,SIGNAL(updatedActivity()),SLOT(updatePrivacyTab()));
	connect(tab_main,SIGNAL(currentChanged(int)),SLOT(tabChanged(int)));
	connect(pb_privacy, SIGNAL(clicked()), SLOT(privacyClicked()));
	connect(pb_removeBlock, SIGNAL(clicked()), SLOT(removeBlockClicked()));
	connect(pb_addBlock, SIGNAL(clicked()), SLOT(addBlockClicked()));
	// FIXME: Temporarily disabling blocking
	pb_removeBlock->hide();
	pb_addBlock->hide();

	// QWhatsThis helpers
	ck_plain->setWhatsThis(
		tr("Normally, Psi logs in using the <i>digest</i> authentication "
		"method.  Check this box to force a plain text login "
		"to the Jabber server. Use this option only if you have "
		"problems connecting with the normal login procedure, as it "
		"makes your connection potentially vulnerable to "
		"attacks."));
	ck_auto->setWhatsThis(
		tr("Automatically login to this account on Psi startup.  Useful if "
		"you have Psi automatically launched when an Internet "
		"connection is detected."));
	ck_reconn->setWhatsThis(
		tr("Makes Psi try to reconnect if the connection was broken.  "
		"Useful, if you have an unstable connection and have to "
		"reconnect often."));
	ck_log->setWhatsThis(
		tr("Keep a log of message history.  Disable this "
		"option if you want to conserve disk space or if you need "
		"maximum security."));
	ck_keepAlive->setWhatsThis(
		tr("Sends so called \"Keep-alive\" packets periodically.  "
		"It is useful if your connection is set to be "
		"automatically disconnected after a certain period of "
		"inactivity (for example, by your ISP) and you want to keep it "
		"up all the time."));
	ck_ignoreSSLWarnings->setWhatsThis(
		tr("Ignores all the SSL warnings, for example, about "
		"incorrect certificates.  Useful if your server doesn't "
		"use a validated SSL certificate and you are "
		"annoyed with warnings."));
	ck_ssl->setWhatsThis(
		tr("Check this option to use an encrypted SSL connection to "
		"the Jabber server.  You may use this option if your "
		"server supports it and if you have the necessary QCA-OpenSSL "
		"plugin installed.  For more information, check the "
		"Psi homepage."));
	ck_compress->setWhatsThis(
		tr("Check this option to use a compressed connection to "
		"the Jabber server, if the server supports it."));
	ck_host->setWhatsThis(
		tr("Use this option for manual configuration of your Jabber host "
		"if it is not the same as the host you are connecting to.  This option is mostly useful "
		"if you have some sort of proxy route on your "
		"local machine (i.e. you connect to localhost), but your "
		"account is registered on an external server."));
	le_resource->setWhatsThis(
		tr("You can have multiple clients connected to the Jabber server "
		"with your single account.  Each login is distinguished by a \"resource\" "
		"name, which you can specify in this field."));
	ck_authzid->setWhatsThis(
		tr("This option sets the Jabber ID of the user you want to "
			"authenticate as. This overrides the Jabber ID you are logging in "
			"as."));
	le_priority->setWhatsThis(
		tr("<p>You can have multiple clients connected to the Jabber "
		"server with your single account.  In such a situation, "
		"the client with the highest priority (that is specified in "
		"this field) will be the one that will receive "
		"all incoming events.</p>"
		"<p>For example, if you have a permanent connection "
		"to the Internet at your work location, and have a dial-up at home, "
		"you can have your Jabber client permanently running at work "
		"with a low priority, and you can still use the same account "
		"from home, using a client with higher priority to "
		"temporary \"disable\" the lower priority client at work.</p>"));

	// Hiding of UI components
	if (PsiOptions::instance()->getOption("options.ui.account.single").toBool()) {
		le_name->hide();
		lb_name->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
		lb_example->hide();
		lb_jid->setText(tr("Username:"));
	}
	
	if (!PsiOptions::instance()->getOption("options.pgp.enable").toBool()) {
		gb_pgp->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.privacy.show").toBool()) 
		tab_main->removeTab(tab_main->indexOf(tab_privacy));
	
	if (!PsiOptions::instance()->getOption("options.ui.account.proxy.show").toBool()) 
		gb_proxy->hide();

	if (!PsiOptions::instance()->getOption("options.ui.account.manual-host").toBool()) {
		ck_host->hide();
		lb_host->hide();
		le_host->hide();
		lb_port->hide();
		le_port->hide();
		ck_ssl->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.ignore-ssl-warnings").toBool()) 
		ck_ignoreSSLWarnings->hide();
	
	if (!PsiOptions::instance()->getOption("options.ui.account.keepalive").toBool()) 
		ck_keepAlive->hide();
	
	if (!PsiOptions::instance()->getOption("options.ui.account.legacy-ssl-probe").toBool()) 
		ck_legacy_ssl_probe->hide();

	if (!PsiOptions::instance()->getOption("options.ui.account.security.show").toBool()) {
		ck_plain->hide();
		ck_req_mutual->hide();
		lb_security_level->hide();
		cb_security_level->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.security.show").toBool() && !PsiOptions::instance()->getOption("options.ui.account.legacy-ssl-probe").toBool() && !PsiOptions::instance()->getOption("options.ui.account.keepalive").toBool() && !PsiOptions::instance()->getOption("options.ui.account.ignore-ssl-warnings").toBool() && !PsiOptions::instance()->getOption("options.ui.account.manual-host").toBool()) {
		gb_advanced->hide();
		if (!PsiOptions::instance()->getOption("options.ui.account.proxy.show").toBool()) {
			tab_main->removeTab(tab_main->indexOf(tab_connection));
		}
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.resource").toBool()) {
		ck_automatic_resource->hide();
		lb_resource->hide();
		le_resource->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.authzid").toBool()) {
		ck_authzid->hide();
		le_authzid->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.priority").toBool()) {
		lb_priority->hide();
		le_priority->hide();
	}
	
	if (!PsiOptions::instance()->getOption("options.ui.account.data-proxy").toBool()) {
		lb_dtProxy->hide();
		le_dtProxy->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.resource").toBool() && !PsiOptions::instance()->getOption("options.ui.account.priority").toBool() && !PsiOptions::instance()->getOption("options.ui.account.data-proxy").toBool()) {
		tab_main->removeTab(tab_main->indexOf(tab_misc));
	}
	

	resize(minimumSize());
}

AccountModifyDlg::~AccountModifyDlg()
{
	pa->dialogUnregister(this);
}

void AccountModifyDlg::updateUserID()
{
	if(key.isNull()) {
		setKeyID(false);
	}
	else {
		setKeyID(true, key.primaryUserId());
	}
}

void AccountModifyDlg::setKeyID(bool b, const QString &s)
{
	if(b) {
		lb_keyname->setText(s);
		lb_keyname->setMinimumWidth(100);
		lb_keyicon->setEnabled(true);
		lb_keyname->setEnabled(true);
		pb_keyclear->setEnabled(true);
	}
	else {
		lb_keyname->setText(tr("No Key Selected"));
		lb_keyicon->setEnabled(false);
		lb_keyname->setEnabled(false);
		pb_keyclear->setEnabled(false);
	}
}

//void AccountModifyDlg::pgpToggled(bool b)
//{
//	if(b) {
//		gb_pgp->setEnabled(true);
//	}
//	else {
//		gb_pgp->setEnabled(false);
//	}
//	updateUserID();
//}

void AccountModifyDlg::setPassword(const QString &pw)
{
	if (!le_pass->text().isEmpty())
		le_pass->setText(pw);
}

void AccountModifyDlg::sslToggled(bool on)
{
	if (on && !checkSSL()) {
		ck_ssl->setChecked(false);
		return;
	}

	le_port->setText(on ? "5223": "5222");
}

bool AccountModifyDlg::checkSSL()
{
	if(!QCA::isSupported("tls")) {
		QMessageBox::information(this, tr("SSL error"), tr("Cannot enable SSL/TLS.  Plugin not found."));
		return false;
	}
	return true;
}

void AccountModifyDlg::hostToggled(bool on)
{
	le_host->setEnabled(on);
	lb_host->setEnabled(on);
	le_port->setEnabled(on);
	lb_port->setEnabled(on);
	ck_ssl->setEnabled(on);
	ck_legacy_ssl_probe->setEnabled(!on);
}

void AccountModifyDlg::chooseKey()
{
	// Show the key dialog
	QString id = (key.isNull() ? "" : key.keyId());
	PGPKeyDlg *w = new PGPKeyDlg(PGPKeyDlg::Secret, id, this);
	w->setWindowTitle(tr("Secret Key"));
	int r = w->exec();
	QCA::KeyStoreEntry entry;
	if(r == QDialog::Accepted)
		entry = w->keyStoreEntry();
	delete w;

	if(!entry.isNull()) {
		key = entry.pgpSecretKey();
		updateUserID();
	}
}

void AccountModifyDlg::clearKey()
{
	setKeyID(false);
	key = QCA::PGPKey();
}

void AccountModifyDlg::detailsVCard()
{
	pa->changeVCard();
}

void AccountModifyDlg::detailsChangePW()
{
	pa->changePW();
}

void AccountModifyDlg::save()
{
	UserAccount acc = pa->userAccount();

	if(le_name->text().isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must specify a name for the account before you may save it."));
		return;
	}

	Jid newJid( JIDUtil::accountFromString(le_jid->text().trimmed()) );
	if ( newJid.user().isEmpty() || newJid.host().isEmpty() ) {
		if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
			QMessageBox::information(this, tr("Error"), tr("<i>Username</i> is invalid."));
		}
		else {
			QMessageBox::information(this, tr("Error"), tr("<i>Jabber ID</i> must be specified in the format <i>user@host</i>."));
		}
		return;
	}

	// do not allow duplicate account names
	QString def = le_name->text();
	QString aname = def;
	int n = 0;
	{
		foreach(PsiAccount* pa, pa->psi()->contactList()->accounts())
			if(aname == pa->name())
				n++;
	}

	if ( aname == acc.name )
		n--;

	if ( n )
		aname = def + '_' + QString::number(++n);
	le_name->setText( aname );

	acc.name = le_name->text();
	acc.jid = JIDUtil::accountFromString(le_jid->text().trimmed()).bare();
	acc.pass = le_pass->text();
	acc.opt_pass = !acc.pass.isEmpty();

	acc.opt_host = ck_host->isChecked();
	acc.host = le_host->text();
	acc.port = le_port->text().toInt();

	acc.req_mutual_auth = ck_req_mutual->isChecked();
	acc.legacy_ssl_probe = ck_legacy_ssl_probe->isChecked();
	acc.security_level = cb_security_level->itemData(cb_security_level->currentIndex()).toInt();

	acc.opt_automatic_resource = ck_automatic_resource->isChecked();
	acc.resource = le_resource->text();
	acc.priority = le_priority->text().toInt();
	
	acc.useAuthzid = ck_authzid->isChecked();
	acc.authzid = le_authzid->text();

	acc.opt_ssl = ck_ssl->isChecked();
	acc.opt_plain = ck_plain->isChecked();
	acc.opt_compress = ck_compress->isChecked();
	acc.opt_auto = ck_auto->isChecked();
	acc.opt_reconn = ck_reconn->isChecked();
	acc.opt_log = ck_log->isChecked();
	acc.opt_keepAlive = ck_keepAlive->isChecked();
	acc.opt_ignoreSSLWarnings = ck_ignoreSSLWarnings->isChecked();
	acc.dtProxy = le_dtProxy->text();

	acc.pgpSecretKey = key;

	acc.proxy_index = pc->currentItem();

	if(pa->isActive()) {
		QMessageBox::information(this, tr("Warning"), tr("This account is currently active, so certain changes may not take effect until the next login."));
	}

	pa->setUserAccount(acc);

	accept();
}

void AccountModifyDlg::tabChanged(int)
{
	updatePrivacyTab();
}

void AccountModifyDlg::addBlockClicked()
{
	bool ok;
	QString input = QInputDialog::getText(NULL, tr("Block contact"), tr("Enter the Jabber ID of the contact to block:"), QLineEdit::Normal, "", &ok);
	Jid jid(input);
	if (ok && !jid.isEmpty()) {
		privacyModel.list().insertItem(0,PrivacyListItem::blockItem(jid.full()));
		privacyModel.reset();
		pa->privacyManager()->changeList(privacyModel.list());
	}
}

void AccountModifyDlg::removeBlockClicked()
{
	if (lv_blocked->currentIndex().isValid()) {
		QModelIndex idx = privacyBlockedModel.mapToSource(lv_blocked->currentIndex());
		privacyModel.removeRow(idx.row(),idx.parent());
		pa->privacyManager()->changeList(privacyModel.list());
	}
}

void AccountModifyDlg::privacyClicked()
{
	PrivacyDlg *d = new PrivacyDlg(pa,this);
	d->show();
}

void AccountModifyDlg::updatePrivacyTab()
{
	if (tab_main->currentWidget() == tab_privacy) {
		if (pa->loggedIn()) {
			if (!privacyInitialized) {
				lb_privacyStatus->setText(tr("Retrieving blocked contact list ..."));
				setPrivacyTabEnabled(false);
				pa->privacyManager()->getDefaultList();
			}
			//else {
			//	setPrivacyTabEnabled(true);
			//}
		}
		else {
			lb_privacyStatus->setText(tr("You are not connected."));
			privacyInitialized = false;
			setPrivacyTabEnabled(false);
		}
	}
}

void AccountModifyDlg::setPrivacyTabEnabled(bool b)
{
	ws_privacy->setCurrentWidget(b ? pg_privacy : pg_privacyStatus);
}

void AccountModifyDlg::updateBlockedContacts(const PrivacyList& l)
{
	privacyInitialized = true;
	privacyModel.setList(l);
	lb_customPrivacy->setVisible(!l.onlyBlockItems());
	setPrivacyTabEnabled(true);
}

void AccountModifyDlg::changeList_error()
{
	privacyInitialized = false;
	updatePrivacyTab();
}

void AccountModifyDlg::getDefaultList_error()
{
	privacyInitialized = true;
	lb_privacyStatus->setText(tr("Your server does not support blocking."));
	setPrivacyTabEnabled(false);
}

