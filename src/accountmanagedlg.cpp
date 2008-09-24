/*
 * accountmanagedlg.cpp - dialogs for manipulating PsiAccounts
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
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <q3listview.h>
#include <q3buttongroup.h>
#include "psicon.h"
#include "psiaccount.h"
#include "common.h"
#include "xmpp_tasks.h"
#include "pgputil.h"
#include "proxy.h"
#include "miniclient.h"
#include "accountadddlg.h"
#include "accountmanagedlg.h"
#include "ui_accountremove.h"
#include "psicontactlist.h"

using namespace XMPP;


//----------------------------------------------------------------------------
// AccountRemoveDlg
//----------------------------------------------------------------------------

class AccountRemoveDlg : public QDialog, public Ui::AccountRemove
{
	Q_OBJECT
public:
	AccountRemoveDlg(ProxyManager *, const UserAccount &, QWidget *parent=0);
	~AccountRemoveDlg();

protected:
	// reimplemented
	//void closeEvent(QCloseEvent *);

public slots:
	void done(int);

private slots:
	void remove();
	void bg_clicked(int);

	void client_handshaken();
	void client_error();
	void client_disconnected();
	void unreg_finished();

private:
	class Private;
	Private *d;

	MiniClient *client;
};

class AccountRemoveDlg::Private
{
public:
	Private() {}

	UserAccount acc;
	QButtonGroup *bg;
	ProxyManager *proxyman;
};

AccountRemoveDlg::AccountRemoveDlg(ProxyManager *proxyman, const UserAccount &acc, QWidget *parent)
:QDialog(parent)
{
  	setupUi(this);
	setModal(false);
	d = new Private;
	d->acc = acc;
	d->proxyman = proxyman;

	setWindowTitle(CAP(caption()));

	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_remove, SIGNAL(clicked()), SLOT(remove()));

	d->bg = new QButtonGroup(0);
	d->bg->addButton(rb_remove, 0);
	d->bg->addButton(rb_removeAndUnreg, 1);
	connect(d->bg, SIGNAL(buttonClicked(int)), SLOT(bg_clicked(int)));
	rb_remove->setChecked(true);
	bg_clicked(0);

	pb_close->setFocus();

	client = new MiniClient;
	connect(client, SIGNAL(handshaken()), SLOT(client_handshaken()));
	connect(client, SIGNAL(error()), SLOT(client_error()));
	connect(client, SIGNAL(disconnected()), SLOT(client_disconnected()));
}

AccountRemoveDlg::~AccountRemoveDlg()
{
	delete client;

	delete d->bg;
	delete d;
}

/*void AccountRemoveDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void AccountRemoveDlg::done(int r)
{
	if(busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel the unregistration?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	QDialog::done(r);
}

void AccountRemoveDlg::bg_clicked(int x)
{
	if(x == 0) {
		lb_pass->setEnabled(false);
		le_pass->setEnabled(false);
	}
	else if(x == 1) {
		lb_pass->setEnabled(true);
		le_pass->setEnabled(true);
		le_pass->setFocus();
	}
}

void AccountRemoveDlg::remove()
{
	bool unreg = rb_removeAndUnreg->isChecked();

	if(unreg) {
		if(!d->acc.pass.isEmpty() && le_pass->text() != d->acc.pass) {
			QMessageBox::information(this, tr("Error"), tr("Password does not match account.  Please try again."));
			le_pass->setFocus();
			return;
		}
	}

	int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to remove <b>%1</b> ?").arg(d->acc.name), tr("&Yes"), tr("&No"));
	if(n != 0)
		return;

	if(!unreg) {
		accept();
		return;
	}

	busy->start();
	gb_account->setEnabled(false);
	pb_remove->setEnabled(false);

	QString pass = le_pass->text();
	Jid j(Jid(d->acc.jid).withResource(d->acc.resource));
	client->connectToServer(j, d->acc.legacy_ssl_probe, d->acc.ssl == UserAccount::SSL_Legacy, d->acc.ssl == UserAccount::SSL_Yes, d->acc.opt_host ? d->acc.host : QString(), d->acc.port, d->proxyman, d->acc.proxyID, &pass);
}

void AccountRemoveDlg::client_handshaken()
{
	// Workaround for servers that do not send a response to the remove request
	client->setErrorOnDisconnect(false);

	// try to unregister an account
	JT_Register *reg = new JT_Register(client->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(unreg_finished()));
	reg->unreg();
	reg->go(true);
}

void AccountRemoveDlg::client_error()
{
	busy->stop();
	gb_account->setEnabled(true);
	pb_remove->setEnabled(true);
}

void AccountRemoveDlg::unreg_finished()
{
	JT_Register *reg = (JT_Register *)sender();

	client->close();
	busy->stop();

	if(reg->success()) {
		QMessageBox::information(this, tr("Success"), tr("The account was unregistered successfully."));
		accept();
		return;
	}
	else if(reg->statusCode() != Task::ErrDisc) {
		gb_account->setEnabled(true);
		pb_remove->setEnabled(true);
		QMessageBox::critical(this, tr("Error"), QString(tr("There was an error unregistering the account.\nReason: %1")).arg(reg->statusString()));
	}
}

void AccountRemoveDlg::client_disconnected()
{
	// Workaround for servers that do not send a response to the remove request
	busy->stop();
	QMessageBox::information(this, tr("Success"), tr("The account was unregistered successfully."));
	accept();
}

//----------------------------------------------------------------------------
// AccountManageDlg
//----------------------------------------------------------------------------
class AccountManageItem : public Q3CheckListItem
{
public:
	AccountManageItem(Q3ListView *par, PsiAccount *_pa)
	:Q3CheckListItem(par,"",CheckBox)
	{
		pa = _pa;
		updateInfo();
	}

	void updateInfo()
	{
		const UserAccount &acc = pa->userAccount();
		setText(0, pa->name());
		//setPixmap(0, IconsetFactory::icon("psi/account"));
		Jid j = acc.jid;
		setText(1, acc.opt_host && acc.host.length() ? acc.host : j.domain());
		setText(2, pa->isActive() ? AccountManageDlg::tr("Active") : AccountManageDlg::tr("Not active"));
		setOn(pa->enabled());
	}

	void stateChange( bool s)
	{
		if (pa->enabled()!=s)
			pa->setEnabled(s);
		updateInfo();
	}

	int rtti() const
	{
		return 8109;
	}

	PsiAccount *pa;
};

AccountManageDlg::AccountManageDlg(PsiCon *_psi)
:QDialog(0)
{
  	setupUi(this);
	setModal(false);
	psi = _psi;
	psi->dialogRegister(this);

	setWindowTitle(CAP(caption()));

	// setup signals
	connect(pb_add, SIGNAL(clicked()), SLOT(add()));
	connect(pb_modify, SIGNAL(clicked()), SLOT(modify()));
	connect(pb_remove, SIGNAL(clicked()), SLOT(remove()));
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));

	connect(lv_accs, SIGNAL(doubleClicked(Q3ListViewItem *)), SLOT(modify(Q3ListViewItem *)));
	connect(lv_accs, SIGNAL(selectionChanged(Q3ListViewItem *)), SLOT(qlv_selectionChanged(Q3ListViewItem *)));
	connect(psi, SIGNAL(accountAdded(PsiAccount *)), SLOT(accountAdded(PsiAccount *)));
	connect(psi, SIGNAL(accountUpdated(PsiAccount *)), SLOT(accountUpdated(PsiAccount *)));
	connect(psi, SIGNAL(accountRemoved(PsiAccount *)), SLOT(accountRemoved(PsiAccount *)));

	lv_accs->setAllColumnsShowFocus(true);
	lv_accs->setResizeMode(Q3ListView::LastColumn);

	foreach(PsiAccount* pa, psi->contactList()->accounts())
		new AccountManageItem(lv_accs, pa);

	if(lv_accs->childCount() > 0)
		lv_accs->setSelected(lv_accs->firstChild(), true);
	else
		qlv_selectionChanged(0);
}

AccountManageDlg::~AccountManageDlg()
{
	psi->dialogUnregister(this);
}

void AccountManageDlg::qlv_selectionChanged(Q3ListViewItem *lvi)
{
	AccountManageItem *i = (AccountManageItem *)lvi;
	bool ok = i ? true: false;

	pb_modify->setEnabled(ok);
	pb_remove->setEnabled(ok);
}

void AccountManageDlg::add()
{
	AccountAddDlg *w = new AccountAddDlg(psi, 0);
	w->show();
}

void AccountManageDlg::modify()
{
	modify(lv_accs->currentItem());
}

void AccountManageDlg::modify(Q3ListViewItem *lvi)
{
	AccountManageItem *i = (AccountManageItem *)lvi;
	if(!i)
		return;

	i->pa->modify();
}

void AccountManageDlg::remove()
{
	AccountManageItem *i = (AccountManageItem *)lv_accs->currentItem();
	if(!i)
		return;

	if(i->pa->isActive()) {
		QMessageBox::information(this, tr("Error"), tr("Unable to remove the account, as it is currently active."));
		return;
	}

	AccountRemoveDlg *w = new AccountRemoveDlg(psi->proxy(), i->pa->userAccount());
	int n = w->exec();
	if(n != QDialog::Accepted) {
		delete w;
		return;
	}
	delete w;
	psi->removeAccount(i->pa);
}

void AccountManageDlg::accountAdded(PsiAccount *pa)
{
	new AccountManageItem(lv_accs, pa);
}

void AccountManageDlg::accountUpdated(PsiAccount *pa)
{
	AccountManageItem *i;
	Q3ListViewItemIterator it(lv_accs);
	for(; (i = (AccountManageItem *)it.current()) ; ++it) {
		if(i->pa == pa)
			break;
	}
	if(!i)
		return;

	i->updateInfo();
}

void AccountManageDlg::accountRemoved(PsiAccount *pa)
{
	AccountManageItem *i;
	Q3ListViewItemIterator it(lv_accs);
	for(; (i = (AccountManageItem *)it.current()) ; ++it) {
		if(i->pa == pa)
			break;
	}
	if(!i)
		return;

	delete i;

	qlv_selectionChanged(lv_accs->currentItem());
}

#include "accountmanagedlg.moc"

