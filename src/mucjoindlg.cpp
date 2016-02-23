/*
 * mucjoindlg.cpp
 * Copyright (C) 2001-2008  Justin Karneges, Michail Pishchagin
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

#include <QString>
#include <QStringList>
#include <QMessageBox>

#include "jidutil.h"
#include "psicon.h"
#include "accountscombobox.h"
#include "psiaccount.h"
#include "mucjoindlg.h"
#include "psicontactlist.h"
#include "groupchatdlg.h"

static const int nickConflictCode = 409;
static const QString additionalSymbol = "_";

MUCJoinDlg::MUCJoinDlg(PsiCon* psi, PsiAccount* pa)
	: QDialog(0)
	, nickAlreadyCompleted_(false)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
	setModal(false);
	ui_.setupUi(this);
	controller_ = psi;
	account_ = 0;
	controller_->dialogRegister(this);
	ui_.ck_history->setChecked(true);
	ui_.ck_history->hide();
	joinButton_ = ui_.buttonBox->addButton(tr("&Join"), QDialogButtonBox::AcceptRole);
	joinButton_->setDefault(true);

	reason_ = MucCustomJoin;

	updateIdentity(pa);

	ui_.cb_ident->setController(controller_);
	ui_.cb_ident->setOnlineOnly(true);
	connect(ui_.cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
	ui_.cb_ident->setAccount(pa);

	connect(controller_, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	foreach(QString j, controller_->recentGCList()) {
		Jid jid(j);
		QString s = tr("%1 on %2").arg(jid.resource()).arg(JIDUtil::toString(jid, false));
		ui_.cb_recent->addItem(s);
		ui_.cb_recent->setItemData(ui_.cb_recent->count()-1, QVariant(j));
	}

	setWindowTitle(CAP(windowTitle()));
	connect(ui_.cb_recent, SIGNAL(activated(int)), SLOT(recent_activated(int)));
	if (!ui_.cb_recent->count()) {
		ui_.cb_recent->setEnabled(false);
		ui_.le_host->setFocus();
	}
	else {
		recent_activated(0);
	}

	setWidgetsEnabled(true);
	adjustSize();
}

MUCJoinDlg::~MUCJoinDlg()
{
	if (controller_)
		controller_->dialogUnregister(this);
	if (account_)
		account_->dialogUnregister(this);
}

void MUCJoinDlg::done(int r)
{
	if (ui_.busy->isActive()) {
		//int n = QMessageBox::information(0, tr("Warning"), tr("Are you sure you want to cancel joining groupchat?"), tr("&Yes"), tr("&No"));
		//if(n != 0)
		//	return;
		account_->groupChatLeave(jid_.domain(), jid_.node());
	}
	QDialog::done(r);
}

void MUCJoinDlg::updateIdentity(PsiAccount *pa)
{
	if (account_)
		disconnect(account_, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));

	account_ = pa;
	joinButton_->setEnabled(account_);

	if (!account_) {
		ui_.busy->stop();
		return;
	}

	connect(account_, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));
}

void MUCJoinDlg::updateIdentityVisibility()
{
	bool visible = controller_->contactList()->enabledAccounts().count() > 1;
	ui_.cb_ident->setVisible(visible);
	ui_.lb_identity->setVisible(visible);
}

void MUCJoinDlg::pa_disconnected()
{
	if (ui_.busy->isActive()) {
		ui_.busy->stop();
	}
}

void MUCJoinDlg::recent_activated(int x)
{
	Jid jid(ui_.cb_recent->itemData(x).toString());
	if (jid.full().isEmpty())
		return;

	ui_.le_host->setText(jid.domain());
	ui_.le_room->setText(jid.node());
	ui_.le_nick->setText(jid.resource());
}

void MUCJoinDlg::doJoin(MucJoinReason r)
{
	if (!account_ || !account_->checkConnected(this))
		return;

	reason_ = r;

	QString host = ui_.le_host->text();
	QString room = ui_.le_room->text();
	QString nick = ui_.le_nick->text();
	QString pass = ui_.le_pass->text();

	if (host.isEmpty() || room.isEmpty() || nick.isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields in order to join."));
		return;
	}

	Jid j = room + '@' + host + '/' + nick;
	if (!j.isValid()) {
		QMessageBox::information(this, tr("Error"), tr("You entered an invalid room name."));
		return;
	}

	GCMainDlg *gc = account_->findDialog<GCMainDlg*>(j.bare());
	if (gc) {
		if(gc->isHidden() && !gc->isTabbed())
			gc->ensureTabbedCorrectly();
		gc->bringToFront();
		if (gc->isInactive()) {
			if(gc->jid() != j)
				gc->setJid(j);
			gc->reactivate();
		}
		joined();
		return;
	}


	if (!account_->groupChatJoin(host, room, nick, pass, !ui_.ck_history->isChecked())) {
		QMessageBox::information(this, tr("Error"), tr("You are in or joining this room already!"));
		return;
	}

	controller_->dialogUnregister(this);
	jid_ = room + '@' + host + '/' + nick;
	account_->dialogRegister(this, jid_);

	setWidgetsEnabled(false);
	ui_.busy->start();
}

void MUCJoinDlg::setWidgetsEnabled(bool enabled)
{
	ui_.cb_ident->setEnabled(enabled);
	ui_.cb_recent->setEnabled(enabled && ui_.cb_recent->count() > 0);
	ui_.gb_info->setEnabled(enabled);
	joinButton_->setEnabled(enabled);
}

void MUCJoinDlg::joined()
{
	controller_->recentGCAdd(jid_.full());
	ui_.busy->stop();

	nickAlreadyCompleted_ = false;
	closeDialogs(this);
	deleteLater();
	account_->addMucItem(jid_.bare());
}

void MUCJoinDlg::error(int error, const QString &str)
{
	ui_.busy->stop();
	setWidgetsEnabled(true);

	account_->dialogUnregister(this);
	controller_->dialogRegister(this);

	if(!nickAlreadyCompleted_ && reason_ == MucAutoJoin && error == nickConflictCode) {
		nickAlreadyCompleted_ = true;
		ui_.le_nick->setText(ui_.le_nick->text() + additionalSymbol);
		doJoin(reason_);
		return;
	}

	if(!isVisible())
		show();

	nickAlreadyCompleted_ = false;

	QMessageBox* msg = new QMessageBox(QMessageBox::Information, tr("Error"), tr("Unable to join groupchat.\nReason: %1").arg(str), QMessageBox::Ok, this);
	msg->setAttribute(Qt::WA_DeleteOnClose, true);
	msg->setModal(false);
	msg->show();
}

void MUCJoinDlg::setJid(const Jid& mucJid)
{
	ui_.le_host->setText(mucJid.domain());
	ui_.le_room->setText(mucJid.node());
}

void MUCJoinDlg::setNick(const QString& nick)
{
	ui_.le_nick->setText(nick);
}

void MUCJoinDlg::setPassword(const QString& password)
{
	ui_.le_pass->setText(password);
}

void MUCJoinDlg::accept()
{
	doJoin();
}
