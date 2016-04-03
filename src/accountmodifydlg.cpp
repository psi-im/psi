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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
#include "psicon.h"
#include "proxy.h"
#include "privacymanager.h"
#include "privacydlg.h"
#include "pgpkeydlg.h"
#include "psicontactlist.h"
#include "iconaction.h"
#include "actionlineedit.h"

AccountModifyDlg::AccountModifyDlg(PsiCon *_psi, QWidget *parent)
:QDialog(parent)
{
	acc.name = "";
	setupUi(this);
	setModal(true);
	pa = NULL;
	psi = _psi;
	init();
}

AccountModifyDlg::AccountModifyDlg(PsiAccount *_pa, QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(false);
	setAttribute(Qt::WA_DeleteOnClose);
	pa = _pa;
	psi = pa->psi();
	acc = pa->userAccount();
	init();
}

void AccountModifyDlg::init()
{
	//connect(pa->psi(), SIGNAL(pgpToggled(bool)), SLOT(pgpToggled(bool)));
	if (pa)
		pa->dialogRegister(this);

	setWindowTitle(CAP(windowTitle()));
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/account").icon());
#endif
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);

	le_pass->setEnabled(true);
	le_host->setEnabled(false);
	lb_host->setEnabled(false);
	le_port->setEnabled(false);
	lb_port->setEnabled(false);

	// FIXME: Temporarily removing security level settings
	ck_req_mutual->hide();
	cb_security_level->hide();
	lb_security_level->hide();

	connect(ck_host, SIGNAL(toggled(bool)), SLOT(hostToggled(bool)));
	connect(pb_key, SIGNAL(clicked()), SLOT(chooseKey()));
	connect(pb_keyclear, SIGNAL(clicked()), SLOT(clearKey()));
	connect(buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), SLOT(save()));

	gb_pgp->setEnabled(false);

	if (pa) {
		connect(pb_vcard, SIGNAL(clicked()), SLOT(detailsVCard()));
		connect(pb_changepw, SIGNAL(clicked()), SLOT(detailsChangePW()));
	}
	else {
		pb_vcard->setEnabled(false);
		pb_changepw->setEnabled(false);
	}

	// Hide the name if necessary
	le_name->setText(acc.name);
	le_jid->setText(JIDUtil::accountToString(acc.jid,false));

	cb_ssl->addItem(tr("Always"),UserAccount::SSL_Yes);
	cb_ssl->addItem(tr("When available"),UserAccount::SSL_Auto);
	cb_ssl->addItem(tr("Never"), UserAccount::SSL_No);
	cb_ssl->addItem(tr("Legacy SSL"), UserAccount::SSL_Legacy);
	cb_ssl->setCurrentIndex(cb_ssl->findData(acc.ssl));
	connect(cb_ssl, SIGNAL(activated(int)), SLOT(sslActivated(int)));

	cb_plain->addItem(tr("Always"),ClientStream::AllowPlain);
	cb_plain->addItem(tr("Over encrypted connection"), ClientStream::AllowPlainOverTLS);
	cb_plain->addItem(tr("Never"), ClientStream::NoAllowPlain);
	cb_plain->setCurrentIndex(cb_plain->findData(acc.allow_plain));

	if (acc.opt_pass)
		le_pass->setText(acc.pass);

	ck_host->setChecked(acc.opt_host);
	le_host->setText(acc.host);
	le_port->setText(QString::number(acc.port));
	ck_req_mutual->setChecked(acc.req_mutual_auth);

	connect(cb_resource, SIGNAL(currentIndexChanged(int)), SLOT(resourceCbChanged(int)));
	connect(cb_priority, SIGNAL(currentIndexChanged(int)), SLOT(priorityCbChanged(int)));
	connect(ck_auto, SIGNAL(clicked()), SLOT(autoconnectCksChanged()));
	connect(ck_connectAfterSleep, SIGNAL(clicked()), SLOT(autoconnectCksChanged()));
	cb_resource->addItem(tr("Manual"));
	cb_resource->addItem(tr("Use host name"));
	cb_resource->setCurrentIndex(acc.opt_automatic_resource ? 1 : 0);
	le_resource->setText(acc.resource);
	cb_priority->addItem(tr("Fixed"));
	cb_priority->addItem(tr("Depends on status"));
	cb_priority->setCurrentIndex(acc.priority_dep_on_status ? 1 : 0);
	sb_priority->setValue(acc.priority);

	connect(ck_custom_auth,SIGNAL(toggled(bool)), lb_authid, SLOT(setEnabled(bool)));
	connect(ck_custom_auth,SIGNAL(toggled(bool)), le_authid, SLOT(setEnabled(bool)));
	connect(ck_custom_auth,SIGNAL(toggled(bool)), lb_realm, SLOT(setEnabled(bool)));
	connect(ck_custom_auth,SIGNAL(toggled(bool)), le_realm, SLOT(setEnabled(bool)));
	ck_custom_auth->setChecked(acc.customAuth);
	le_authid->setText(acc.authid);
	le_realm->setText(acc.realm);

	ck_compress->setChecked(acc.opt_compress);
	ck_auto->setChecked(acc.opt_auto);
	ck_reconn->setChecked(acc.opt_reconn);
	ck_connectAfterSleep->setChecked(acc.opt_connectAfterSleep);
	ck_autoSameStatus->setChecked(acc.opt_autoSameStatus);
	ck_log->setChecked(acc.opt_log);
	ck_keepAlive->setChecked(acc.opt_keepAlive);
	ck_enableSM->setChecked(acc.opt_sm);
	ck_ibbOnly->setChecked(acc.ibbOnly);
	le_dtProxy->setText(acc.dtProxy.full());

	ActionLineEdit *ale_delStunHost = new ActionLineEdit(cb_stunHost);
	cb_stunHost->setLineEdit(ale_delStunHost);
	IconAction *ia_delStunHost = new IconAction(tr("Delete current host from the list"), this, "psi/remove");
	connect(ia_delStunHost, SIGNAL(triggered()), SLOT(removeStunHost()));
	ale_delStunHost->addAction(ia_delStunHost);

	cb_stunHost->addItem(tr("<don't use>"));
	cb_stunHost->addItems(acc.stunHosts);
	if (acc.stunHost.isEmpty()) {
		cb_stunHost->setCurrentIndex(0);
	}
	else {
		cb_stunHost->setCurrentIndex(cb_stunHost->findText(acc.stunHost));
	}
	le_stunUser->setText(acc.stunUser);
	le_stunPass->setText(acc.stunPass);
	autoconnectCksChanged();
	connect(ck_ibbOnly, SIGNAL(toggled(bool)), SLOT(ibbOnlyToggled(bool)));
	ibbOnlyToggled(acc.ibbOnly);

	ck_scram_salted_password->setChecked(acc.storeSaltedHashedPassword);

	key = acc.pgpSecretKey;
	updateUserID();
	PGPUtil::instance().clearPGPAvailableCache();
	if(PGPUtil::instance().pgpAvailable()) {
		gb_pgp->setEnabled(true);
	}

	pc = ProxyManager::instance()->createProxyChooser(tab_connection);
	replaceWidget(lb_proxychooser, pc);
	pc->setCurrentItem(acc.proxyID);

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
		buttonBox->button(QDialogButtonBox::Save)->setFocus();

	// Privacy
	privacyInitialized = false;
	lb_customPrivacy->hide();
	privacyBlockedModel.setSourceModel(&privacyModel);
	lv_blocked->setModel(&privacyBlockedModel);
	if (pa) {
		connect(pa->privacyManager(),SIGNAL(defaultListAvailable(const PrivacyList&)),SLOT(updateBlockedContacts(const PrivacyList&)));
		connect(pa->privacyManager(),SIGNAL(defaultListError()),SLOT(getDefaultList_error()));
		connect(pa->privacyManager(),SIGNAL(changeList_error()),SLOT(changeList_error()));
		connect(pa,SIGNAL(updatedActivity()),SLOT(updatePrivacyTab()));
	}
	connect(tab_main,SIGNAL(currentChanged(int)),SLOT(tabChanged(int)));
	connect(pb_privacy, SIGNAL(clicked()), SLOT(privacyClicked()));
	connect(pb_removeBlock, SIGNAL(clicked()), SLOT(removeBlockClicked()));
	connect(pb_addBlock, SIGNAL(clicked()), SLOT(addBlockClicked()));
	// FIXME: Temporarily disabling blocking
	pb_removeBlock->hide();
	pb_addBlock->hide();

	// QWhatsThis helpers
	cb_plain->setWhatsThis(
		tr("Normally, Psi logs in using the <i>digest</i> authentication "
		"method.  Check this box to force a plain text login "
		"to the XMPP server. Use this option only if you have "
		"problems connecting with the normal login procedure, as it "
		"makes your connection potentially vulnerable to "
		"attacks."));
	ck_auto->setWhatsThis(
		tr("Automatically login to this account on Psi startup.  Useful if "
		"you have Psi automatically launched when an Internet "
		"connection is detected."));
	ck_connectAfterSleep->setWhatsThis(
		tr("Makes Psi try to connect when the computer resumes "
		"after a sleep."));
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
	ck_enableSM->setWhatsThis(
		tr("Enables Stream Management protocol if possible. It is useful, "
		   "if you have an unstable connection. Your server must support "
		   "this option. To learn more, see XEP-0184."));
	cb_ssl->setWhatsThis(
		tr("Check this option to use an encrypted SSL connection to "
		"the XMPP server.  You may use this option if your "
		"server supports it and if you have the necessary qca-ossl "
		"plugin installed.  For more information, check the "
		"Psi homepage."));
	ck_compress->setWhatsThis(
		tr("Check this option to use a compressed connection to "
		"the XMPP server, if the server supports it."));
	ck_host->setWhatsThis(
		tr("Use this option for manual configuration of your XMPP host "
		"if it is not the same as the host you are connecting to.  This option is mostly useful "
		"if you have some sort of proxy route on your "
		"local machine (i.e. you connect to localhost), but your "
		"account is registered on an external server."));
	le_resource->setWhatsThis(
		tr("You can have multiple clients connected to the XMPP server "
		"with your single account.  Each login is distinguished by a \"resource\" "
		"name, which you can specify in this field."));
	ck_custom_auth->setWhatsThis(
		tr("This option sets the user (and realm) you want to "
			"authenticate as. This overrides the XMPP address you are logging in "
			"as."));
	sb_priority->setWhatsThis(
		tr("<p>You can have multiple clients connected to the XMPP "
		"server with your single account.  In such a situation, "
		"the client with the highest priority (that is specified in "
		"this field) will be the one that will receive "
		"all incoming events.</p>"
		"<p>For example, if you have a permanent connection "
		"to the Internet at your work location, and have a dial-up at home, "
		"you can have your XMPP client permanently running at work "
		"with a low priority, and you can still use the same account "
		"from home, using a client with higher priority to "
		"temporary \"disable\" the lower priority client at work.</p>"));

	// Hiding of UI components
	if ((!pa && acc.name.isEmpty()) || PsiOptions::instance()->getOption("options.ui.account.single").toBool()) {
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

	if (!PsiOptions::instance()->getOption("options.ui.account.proxy.show").toBool()) {
		lb_proxy->hide();
		pc->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.manual-host").toBool()) {
		ck_host->hide();
		lb_host->hide();
		le_host->hide();
		lb_port->hide();
		le_port->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.keepalive").toBool())
		ck_keepAlive->hide();

	if (!PsiOptions::instance()->getOption("options.ui.account.sm.show").toBool())
		ck_enableSM->hide();

	if (!PsiOptions::instance()->getOption("options.ui.account.security.show").toBool()) {
		lb_plain->hide();
		cb_plain->hide();
		ck_req_mutual->hide();
		lb_security_level->hide();
		cb_security_level->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.security.show").toBool() && !PsiOptions::instance()->getOption("options.ui.account.keepalive").toBool() && !PsiOptions::instance()->getOption("options.ui.account.sm.show").toBool() && !PsiOptions::instance()->getOption("options.ui.account.manual-host").toBool() && !PsiOptions::instance()->getOption("options.ui.account.proxy.show").toBool()) {
		tab_main->removeTab(tab_main->indexOf(tab_connection));
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.resource").toBool()) {
		cb_resource->hide();
		lb_resource->hide();
		le_resource->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.custom-authid").toBool()) {
		ck_custom_auth->hide();
		lb_authid->hide();
		le_authid->hide();
		lb_realm->hide();
		le_realm->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.priority").toBool()) {
		lb_priority->hide();
		cb_priority->hide();
		sb_priority->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.data-proxy").toBool()) {
		lb_dtProxy->hide();
		le_dtProxy->hide();
	}

	if (!PsiOptions::instance()->getOption("options.ui.account.resource").toBool() && !PsiOptions::instance()->getOption("options.ui.account.priority").toBool() && !PsiOptions::instance()->getOption("options.ui.account.data-proxy").toBool()) {
		tab_main->removeTab(tab_main->indexOf(tab_misc));
	}

	resize(minimumSizeHint());
}

AccountModifyDlg::~AccountModifyDlg()
{
	if (pa)
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

void AccountModifyDlg::sslActivated(int i)
{
	if ((cb_ssl->itemData(i) == UserAccount::SSL_Yes || cb_ssl->itemData(i) == UserAccount::SSL_Legacy) && !checkSSL()) {
		cb_ssl->setCurrentIndex(cb_ssl->findData(UserAccount::SSL_Auto));
	}
	else if (cb_ssl->itemData(i) == UserAccount::SSL_Legacy && !ck_host->isChecked()) {
		QMessageBox::critical(this, tr("Error"), tr("Legacy SSL is only available in combination with manual host/port."));
		cb_ssl->setCurrentIndex(cb_ssl->findData(UserAccount::SSL_Auto));
	}
}

bool AccountModifyDlg::checkSSL()
{
	if(!QCA::isSupported("tls")) {
		QMessageBox::critical(this, tr("SSL error"), tr("Cannot enable SSL/TLS.  Plugin not found."));
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
	if (!on && cb_ssl->currentIndex() == cb_ssl->findData(UserAccount::SSL_Legacy)) {
		cb_ssl->setCurrentIndex(cb_ssl->findData(UserAccount::SSL_Auto));
	}
}

void AccountModifyDlg::ibbOnlyToggled(bool state)
{
	le_dtProxy->setDisabled(state);
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
	if (pa)
		pa->changeVCard();
}

void AccountModifyDlg::detailsChangePW()
{
	if (pa) {
		pa->changePW();
	}
}

void AccountModifyDlg::removeStunHost()
{
	// Don't remove no host
	if (!cb_stunHost->currentIndex()) {
		return;
	}

	QString host = cb_stunHost->currentText();
	int item = cb_stunHost->findText(host);
	if (item > -1) {
		cb_stunHost->removeItem(item);
	}
	cb_stunHost->setCurrentIndex(0);
}

void AccountModifyDlg::save()
{
	/*if(pa && le_name->text().isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must specify a name for the account before you may save it."));
		return;
	}*/

	Jid newJid( JIDUtil::accountFromString(le_jid->text().trimmed()) );
	if ( newJid.node().isEmpty() || newJid.domain().isEmpty() ) {
		if (!PsiOptions::instance()->getOption("options.account.domain").toString().isEmpty()) {
			QMessageBox::information(this, tr("Error"), tr("<i>Username</i> is invalid."));
		}
		else {
			QMessageBox::information(this, tr("Error"), tr("<i>XMPP Address</i> must be specified in the format <i>user@host</i>."));
		}
		return;
	}

	// do not allow duplicate account names
	if (!pa && le_name->text().isEmpty())
		le_name->setText(newJid.domain());
	QString def = le_name->text();
	QString aname = def;
	int n = 0;
	{
		foreach(PsiAccount* pa, psi->contactList()->accounts())
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
	acc.security_level = cb_security_level->itemData(cb_security_level->currentIndex()).toInt();

	acc.opt_automatic_resource = (cb_resource->currentIndex() == 1);
	acc.resource = le_resource->text();
	acc.priority_dep_on_status = (cb_priority->currentIndex() == 1);
	acc.priority = sb_priority->value();
	acc.customAuth = ck_custom_auth->isChecked();
	acc.authid = le_authid->text();
	acc.realm = le_realm->text();
	acc.ssl =  (UserAccount::SSLFlag) cb_ssl->itemData(cb_ssl->currentIndex()).toInt();
	acc.allow_plain =  (ClientStream::AllowPlainType) cb_plain->itemData(cb_plain->currentIndex()).toInt();
	acc.opt_compress = ck_compress->isChecked();
	acc.opt_auto = ck_auto->isChecked();
	acc.opt_connectAfterSleep = ck_connectAfterSleep->isChecked();
	acc.opt_autoSameStatus = ck_autoSameStatus->isChecked();
	acc.opt_reconn = ck_reconn->isChecked();
	acc.opt_log = ck_log->isChecked();
	acc.opt_keepAlive = ck_keepAlive->isChecked();
	acc.opt_sm = ck_enableSM->isChecked();
	acc.ibbOnly = ck_ibbOnly->isChecked();
	acc.dtProxy = le_dtProxy->text();
	acc.stunHost = cb_stunHost->currentIndex() ? cb_stunHost->currentText().trimmed() : "";
	acc.stunHosts.clear();
	// first item is no host
	for (int i = 1; i < cb_stunHost->count(); i++) {
		acc.stunHosts << cb_stunHost->itemText(i);
	}
	if (acc.stunHosts.indexOf(acc.stunHost) == -1) {
		acc.stunHosts << acc.stunHost;
	}
	acc.stunUser = le_stunUser->text();
	acc.stunPass = le_stunPass->text();

	acc.storeSaltedHashedPassword = ck_scram_salted_password->isChecked();

	acc.pgpSecretKey = key;

	acc.proxyID = pc->currentItem();

	if (pa) {
		pa->setUserAccount(acc);

		if (pa->isActive()) {
			QMessageBox messageBox(QMessageBox::Information, tr("Warning"),
								   tr("This account is currently active, so certain changes may not take effect until the next login."),
								   QMessageBox::NoButton, this);
			QPushButton* cancel = messageBox.addButton(tr("Reconnect &Later"), QMessageBox::RejectRole);
			QPushButton* reconnect = messageBox.addButton(tr("Reconnect &Now"), QMessageBox::AcceptRole);
			messageBox.setDefaultButton(reconnect);
			messageBox.exec();
			Q_UNUSED(cancel);
			if (messageBox.clickedButton() == reconnect) {
				XMPP::Status status = pa->status();
				pa->setStatus(XMPP::Status::Offline);
				pa->setStatus(status);
			}
		}
		pa->reconfigureFTManager();
	}
	else {
		psi->contactList()->createAccount(acc);
	}

	accept();
}

void AccountModifyDlg::tabChanged(int)
{
	updatePrivacyTab();
}

void AccountModifyDlg::addBlockClicked()
{
	if (!pa)
		return;

	bool ok;
	QString input = QInputDialog::getText(NULL, tr("Block contact"), tr("Enter the XMPP Address of the contact to block:"), QLineEdit::Normal, "", &ok);
	Jid jid(input);
	if (ok && !jid.isEmpty()) {
		privacyModel.insertItem(0, PrivacyListItem::blockItem(jid.full()));
		pa->privacyManager()->changeList(privacyModel.list());
	}
}

void AccountModifyDlg::removeBlockClicked()
{
	if (!pa)
		return;

	if (lv_blocked->currentIndex().isValid()) {
		QModelIndex idx = privacyBlockedModel.mapToSource(lv_blocked->currentIndex());
		privacyModel.removeRow(idx.row(),idx.parent());
		pa->privacyManager()->changeList(privacyModel.list());
	}
}

void AccountModifyDlg::privacyClicked()
{
	PrivacyDlg *d = new PrivacyDlg(pa->name(), pa->privacyManager(), this);
	d->show();
}

void AccountModifyDlg::updatePrivacyTab()
{
	if (tab_main->currentWidget() == tab_privacy) {
		if (pa && pa->loggedIn()) {
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

void AccountModifyDlg::resourceCbChanged(int index)
{
	le_resource->setEnabled(index == 0);
}

void AccountModifyDlg::priorityCbChanged(int index)
{
	sb_priority->setEnabled(index == 0);
}

void AccountModifyDlg::autoconnectCksChanged()
{
	if (ck_auto->isChecked() || ck_connectAfterSleep->isChecked()) {
		ck_autoSameStatus->setEnabled(true);
	}
	else {
		ck_autoSameStatus->setEnabled(false);
	}
}
