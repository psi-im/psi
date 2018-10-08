/*
 * accountmanagedlg.cpp - dialogs for manipulating PsiAccounts
 * Copyright (C) 2001-2009  Justin Karneges, Michail Pishchagin
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
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QHeaderView>
#include <QDropEvent>

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
#include "iconaction.h"
#include "shortcutmanager.h"

using namespace XMPP;


//----------------------------------------------------------------------------
// AccountRemoveDlg
//----------------------------------------------------------------------------

class AccountRemoveDlg : public QDialog, public Ui::AccountRemove
{
	Q_OBJECT
public:
	AccountRemoveDlg(const UserAccount &, QWidget *parent=0);
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
	QPushButton* pb_close_;
	QPushButton* pb_remove_;
};

class AccountRemoveDlg::Private
{
public:
	Private() {}

	UserAccount acc;
	QButtonGroup *bg;
};

AccountRemoveDlg::AccountRemoveDlg(const UserAccount &acc, QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(false);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	d = new Private;
	d->acc = acc;

	setWindowTitle(CAP(windowTitle()));

	pb_close_ = buttonBox->button(QDialogButtonBox::Cancel);
	pb_close_->setDefault(true);
	pb_remove_ = buttonBox->addButton(tr("&Remove"), QDialogButtonBox::DestructiveRole);

	connect(pb_close_, SIGNAL(clicked()), SLOT(close()));
	connect(pb_remove_, SIGNAL(clicked()), SLOT(remove()));

	d->bg = new QButtonGroup(this);
	d->bg->addButton(rb_remove, 0);
	d->bg->addButton(rb_removeAndUnreg, 1);
	connect(d->bg, SIGNAL(buttonClicked(int)), SLOT(bg_clicked(int)));
	rb_remove->setChecked(true);
	bg_clicked(0);

	client = new MiniClient(this);
	connect(client, SIGNAL(handshaken()), SLOT(client_handshaken()));
	connect(client, SIGNAL(error()), SLOT(client_error()));
	connect(client, SIGNAL(disconnected()), SLOT(client_disconnected()));
	adjustSize();
}

AccountRemoveDlg::~AccountRemoveDlg()
{
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
		QMessageBox messageBox(QMessageBox::Information, CAP(tr("Warning")), tr("Are you sure you want to cancel the unregistration?"));
		QPushButton* cancel = messageBox.addButton(tr("&No"), QMessageBox::RejectRole);
		QPushButton* accept = messageBox.addButton(tr("&Yes"), QMessageBox::AcceptRole);
		messageBox.setDefaultButton(accept);
		messageBox.exec();
		if (messageBox.clickedButton() == cancel)
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

	QMessageBox messageBox(QMessageBox::Information, CAP(tr("Warning")), tr("Are you sure you want to remove <b>%1</b> ?").arg(d->acc.name));
	QPushButton* cancel = messageBox.addButton(QMessageBox::Cancel);
	QPushButton* remove = messageBox.addButton(tr("&Remove"), QMessageBox::AcceptRole);
	messageBox.setDefaultButton(remove);
	messageBox.exec();
	if (messageBox.clickedButton() == cancel)
		return;

	if(!unreg) {
		accept();
		return;
	}

	busy->start();
	gb_account->setEnabled(false);
	pb_remove_->setEnabled(false);

	QString pass = le_pass->text();
	Jid j(Jid(d->acc.jid).withResource(d->acc.resource));
	client->connectToServer(j, d->acc.legacy_ssl_probe, d->acc.ssl == UserAccount::SSL_Legacy, d->acc.ssl == UserAccount::SSL_Yes, d->acc.opt_host ? d->acc.host : QString(), d->acc.port, d->acc.proxyID, &pass);
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
	pb_remove_->setEnabled(true);
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
		pb_remove_->setEnabled(true);
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
class AccountManageItem : public QObject, public QTreeWidgetItem
{
	Q_OBJECT
public:
	QPointer<PsiAccount> pa;

public:
	AccountManageItem(QTreeWidget *par, PsiAccount *_pa)
	:QTreeWidgetItem(par)
	{
		pa = _pa;
		Q_ASSERT(!pa.isNull());
		setFlags(flags() ^ Qt::ItemIsDropEnabled);
		connect(pa, SIGNAL(updatedActivity()), SLOT(updateInfo()));
		connect(pa, SIGNAL(updatedAccount()), SLOT(updateInfo()));
		updateInfo();
	}

	void setData (int column, int role, const QVariant& value)
	{
		bool oldChecked = checkState(0) == Qt::Checked;
		QTreeWidgetItem::setData(column, role, value);
		bool checked = checkState(0) == Qt::Checked;
		if (oldChecked != checked && role == Qt::CheckStateRole) {
			if (pa->enabled() != checked)
				pa->setEnabled(checked);
			QTimer::singleShot(0, this, SLOT(updateInfo()));
		}
	}

private slots:
	void updateInfo()
	{
		UserAccount acc = pa->accountOptions();
		Jid j = acc.jid;
		setText(0, pa->name());
		setText(1, acc.opt_host && acc.host.length() ? acc.host : j.domain());
		setText(2, pa->isActive() ? AccountManageDlg::tr("Active") : AccountManageDlg::tr("Not active"));
		setCheckState(0, pa->enabled() ? Qt::Checked : Qt::Unchecked);
	}
};



AccountManageTree::AccountManageTree(QWidget *parent)
	: QTreeWidget(parent)
{

}

void AccountManageTree::dropEvent(QDropEvent *event)
{
	QTreeWidget::dropEvent(event);
	AccountManageTree *tree = qobject_cast<AccountManageTree *>(event->source());

	if (tree) {
		QList<PsiAccount*> accountsList;
		foreach (QTreeWidgetItem *ami, tree->findItems("*", Qt::MatchWildcard)) {
			accountsList.append(((AccountManageItem *)ami)->pa.data());
		}
		emit orderChanged(accountsList);
	}
}

void AccountManageTree::dragMoveEvent(QDragMoveEvent *event)
{
	QTreeWidget::dragMoveEvent(event);
	sortByColumn(-1);
}



AccountManageDlg::AccountManageDlg(PsiCon *_psi)
:QDialog(0)
{
	setupUi(this);
	setModal(false);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	psi = _psi;
	psi->dialogRegister(this);

	setWindowTitle(CAP(windowTitle()));

	removeAction_ = new IconAction("", "psi/remove", QString(), ShortcutManager::instance()->shortcuts("contactlist.delete"), this, "act_remove");
	connect(removeAction_, SIGNAL(triggered()), SLOT(remove()));
	lv_accs->addAction(removeAction_);

	// setup signals
	connect(pb_add, SIGNAL(clicked()), SLOT(add()));
	connect(pb_modify, SIGNAL(clicked()), SLOT(modify()));
	connect(pb_remove, SIGNAL(clicked()), SLOT(remove()));

	connect(lv_accs, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), SLOT(modify(QTreeWidgetItem *)));
	connect(lv_accs, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), SLOT(qlv_selectionChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
	connect(lv_accs, SIGNAL(orderChanged(QList<PsiAccount*>)), psi, SLOT(setAccountsOrder(QList<PsiAccount*>)));
	connect(psi, SIGNAL(accountAdded(PsiAccount *)), SLOT(accountAdded(PsiAccount *)));
	connect(psi, SIGNAL(accountRemoved(PsiAccount *)), SLOT(accountRemoved(PsiAccount *)));
#ifdef HAVE_QT5
	lv_accs->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	lv_accs->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif
	lv_accs->setDragDropMode(QAbstractItemView::InternalMove);
	lv_accs->setDragDropOverwriteMode(false);
	lv_accs->setSortingEnabled(true);
	lv_accs->sortByColumn(-1);

	foreach(PsiAccount* pa, psi->contactList()->accounts())
		new AccountManageItem(lv_accs, pa);

	if (lv_accs->topLevelItemCount())
		lv_accs->setCurrentItem(lv_accs->topLevelItem(0));

	//adjustSize();
}

AccountManageDlg::~AccountManageDlg()
{
	psi->dialogUnregister(this);
}

void AccountManageDlg::qlv_selectionChanged(QTreeWidgetItem *lvi, QTreeWidgetItem *)
{
	AccountManageItem *i = (AccountManageItem *)lvi;
	bool ok = i ? true: false;

	pb_modify->setEnabled(ok);
	pb_remove->setEnabled(ok);
	removeAction_->setEnabled(ok);
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

void AccountManageDlg::modify(QTreeWidgetItem *lvi)
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

	if(i->pa->eventQueue()->count()) {
		QMessageBox::information(0, tr("Error"), qApp->translate("PsiAccount", "Unable to disable the account, as it has pending events."));
		return;
	}

	if(i->pa->isActive()) {
		QMessageBox messageBox(QMessageBox::Information, CAP(tr("Error")), tr("Please disconnect before removing the account."));
		QPushButton* cancel = messageBox.addButton(QMessageBox::Cancel);
		QPushButton* disconnect = messageBox.addButton(tr("&Disconnect"), QMessageBox::AcceptRole);
		messageBox.setDefaultButton(disconnect);
		messageBox.exec();
		if (messageBox.clickedButton() == cancel)
			return;

		i->pa->setStatus(XMPP::Status::Offline);
	}

	AccountRemoveDlg *w = new AccountRemoveDlg(i->pa->userAccount());
	int n = w->exec();
	if(n != QDialog::Accepted) {
		delete w;
		return;
	}
	delete w;
	psi->removeAccount(i->pa);

	activateWindow();
	lv_accs->setFocus();
}

void AccountManageDlg::accountAdded(PsiAccount *pa)
{
	new AccountManageItem(lv_accs, pa);
}

void AccountManageDlg::accountRemoved(PsiAccount *pa)
{
	for (int index = 0; index < lv_accs->topLevelItemCount(); ++index) {
		AccountManageItem* i = static_cast<AccountManageItem*>(lv_accs->topLevelItem(index));
		if(i->pa == pa) {
			delete i;
			qlv_selectionChanged(lv_accs->currentItem(), 0);
			break;
		}
	}
}

#include "accountmanagedlg.moc"
