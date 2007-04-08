/*
 * mucjoindlg.cpp
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

#include <QString>
#include <QStringList>

#include "jidutil.h"
#include "im.h"
#include "psicon.h"
#include "accountscombobox.h"
#include "psiaccount.h"
#include "mucjoindlg.h"

using namespace XMPP;


class MUCJoinDlg::Private
{
public:
	Private() {}

	PsiCon *psi;
	PsiAccount *pa;
	AccountsComboBox *cb_ident;
	BusyWidget *busy;
	QStringList rl;
	Jid jid;
};

MUCJoinDlg::MUCJoinDlg(PsiCon *psi, PsiAccount *pa)
:QDialog(0, Qt::WDestructiveClose)
{
	d = new Private;
	setModal(false);
	setupUi(this);
	d->psi = psi;
	d->pa = 0;
	d->psi->dialogRegister(this);
	d->busy = busy;
	ck_history->setChecked(true);

	updateIdentity(pa);
	
	d->cb_ident = d->psi->accountsComboBox(this,true);
	connect(d->cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
	d->cb_ident->setAccount(pa);
	replaceWidget(lb_ident, d->cb_ident);

	connect(d->psi, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	d->rl = d->psi->recentGCList();
	for(QStringList::ConstIterator it = d->rl.begin(); it != d->rl.end(); ++it) {
		Jid j(*it);
		QString s = tr("%1 on %2").arg(j.resource()).arg(JIDUtil::toString(j,false));
		cb_recent->insertItem(s);
	}

	setWindowTitle(CAP(caption()));
	pb_join->setDefault(true);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(pb_join, SIGNAL(clicked()), SLOT(doJoin()));
	connect(cb_recent, SIGNAL(activated(int)), SLOT(recent_activated(int)));
	if(d->rl.isEmpty()) {
		cb_recent->setEnabled(false);
		le_host->setFocus();
	}
	else
		recent_activated(0);
}

MUCJoinDlg::~MUCJoinDlg()
{
	if(d->psi)
		d->psi->dialogUnregister(this);
	if(d->pa)
		d->pa->dialogUnregister(this);
	delete d;
}

/*void MUCJoinDlg::closeEvent(QCloseEvent *e)
{
	e->ignore();
	reject();
}*/

void MUCJoinDlg::done(int r)
{
	if(d->busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel joining groupchat?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
		d->pa->groupChatLeave(d->jid.host(), d->jid.user());
	}
	QDialog::done(r);
}

void MUCJoinDlg::updateIdentity(PsiAccount *pa)
{
	if(d->pa)
		disconnect(d->pa, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));

	d->pa = pa;
	pb_join->setEnabled(d->pa);

	if(!d->pa) {
		d->busy->stop();
		return;
	}

	connect(d->pa, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));
}

void MUCJoinDlg::updateIdentityVisibility()
{
	bool visible = d->psi->accountList(true).count() > 1;
	d->cb_ident->setVisible(visible);
	lb_identity->setVisible(visible);
}

void MUCJoinDlg::pa_disconnected()
{
	if(d->busy->isActive()) {
		d->busy->stop();
	}
}

void MUCJoinDlg::recent_activated(int x)
{
	int n = 0;
	bool found = false;
	QString str;
	for(QStringList::ConstIterator it = d->rl.begin(); it != d->rl.end(); ++it) {
		if(n == x) {
			found = true;
			str = *it;
			break;
		}
		++n;
	}
	if(!found)
		return;

	Jid j(str);
	le_host->setText(j.host());
	le_room->setText(j.user());
	le_nick->setText(j.resource());
}

void MUCJoinDlg::doJoin()
{
	if(!d->pa->checkConnected(this))
		return;

	QString host = le_host->text();
	QString room = le_room->text();
	QString nick = le_nick->text();
	QString pass = le_pass->text();

	if(host.isEmpty() || room.isEmpty() || nick.isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields in order to join."));
		return;
	}

	Jid j = room + '@' + host + '/' + nick;
	if(!j.isValid()) {
		QMessageBox::information(this, tr("Error"), tr("You entered an invalid room name."));
		return;
	}

	if(!d->pa->groupChatJoin(host, room, nick, pass, !ck_history->isChecked())) {
		QMessageBox::information(this, tr("Error"), tr("You are in or joining this room already!"));
		return;
	}

	d->psi->dialogUnregister(this);
	d->jid = room + '@' + host + '/' + nick;
	d->pa->dialogRegister(this, d->jid);

	disableWidgets();
	d->busy->start();
}

void MUCJoinDlg::disableWidgets()
{
	d->cb_ident->setEnabled(false);
	cb_recent->setEnabled(false);
	gb_info->setEnabled(false);
	pb_join->setEnabled(false);
}

void MUCJoinDlg::enableWidgets()
{
	d->cb_ident->setEnabled(true);
	if(!d->rl.isEmpty())
		cb_recent->setEnabled(true);
	gb_info->setEnabled(true);
	pb_join->setEnabled(true);
}

void MUCJoinDlg::joined()
{
	d->psi->recentGCAdd(d->jid.full());
	d->busy->stop();

	closeDialogs(this);
	deleteLater();
}

void MUCJoinDlg::error(int, const QString &str)
{
	d->busy->stop();
	enableWidgets();

	pb_join->setFocus();

	d->pa->dialogUnregister(this);
	d->psi->dialogRegister(this);

	QMessageBox::information(this, tr("Error"), tr("Unable to join groupchat.\nReason: %1").arg(str));
}


