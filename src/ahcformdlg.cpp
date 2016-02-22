/*
 * ahcformdlg.cpp - Ad-Hoc Command Form Dialog
 * Copyright (C) 2005  Remko Troncon
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

#include "ahcformdlg.h"

#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

#include "ahcommand.h"
#include "ahcexecutetask.h"
#include "xdata_widget.h"
#include "xmpp_client.h"
#include "psiaccount.h"
#include "busywidget.h"

AHCFormDlg::AHCFormDlg(const AHCommand& r, const Jid& receiver, XMPP::Client* client, bool final)
	: QDialog(0)
	, pb_prev_(0)
	, pb_next_(0)
	, pb_complete_(0)
	, pb_cancel_(0)
	, xdata_(0)
	, receiver_(receiver)
	, client_(client)
{
	ui_.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	// Save node
	node_ = r.node();
	sessionId_ = r.sessionId();

	ui_.lb_note->setText(r.note().text);
	ui_.lb_note->setVisible(r.hasNote());

	ui_.lb_instructions->setText(r.data().instructions());
	ui_.lb_instructions->setVisible(!r.data().instructions().isEmpty());

	// XData form
	xdata_ = new XDataWidget(this, client_, receiver);
	xdata_->setForm(r.data(), false);
	ui_.scrollArea->setWidget(xdata_);
	if (!r.hasData() && (r.hasNote() || !r.data().instructions().isEmpty()))
		ui_.scrollArea->setVisible(false);

	ui_.busy->setVisible(!final);

	if (!final) {
		if (r.actions().empty()) {
			// Single stage dialog
			pb_complete_ = ui_.buttonBox->addButton(tr("Finish"), QDialogButtonBox::AcceptRole);
			connect(pb_complete_,SIGNAL(clicked()),SLOT(doExecute()));
		}
		else {
			// Multi-stage dialog

			// Previous
			pb_prev_ = ui_.buttonBox->addButton(tr("Previous"), QDialogButtonBox::ActionRole);
			if (r.actions().contains(AHCommand::Prev)) {
				if (r.defaultAction() == AHCommand::Prev) {
					pb_prev_->setDefault(true);
					pb_prev_->setFocus();
				}
				connect(pb_prev_,SIGNAL(clicked()),SLOT(doPrev()));
				pb_prev_->setEnabled(true);
			}
			else {
				pb_prev_->setEnabled(false);
			}

			// Next
			pb_next_ = ui_.buttonBox->addButton(tr("Next"), QDialogButtonBox::ActionRole);
			if (r.actions().contains(AHCommand::Next)) {
				if (r.defaultAction() == AHCommand::Next) {
					connect(pb_next_,SIGNAL(clicked()),SLOT(doExecute()));
					pb_next_->setDefault(true);
					pb_next_->setFocus();
				}
				else {
					connect(pb_next_,SIGNAL(clicked()),SLOT(doNext()));
				}
				pb_next_->setEnabled(true);
			}
			else {
				pb_next_->setEnabled(false);
			}

			// Complete
			pb_complete_ = ui_.buttonBox->addButton(tr("Finish"), QDialogButtonBox::AcceptRole);
			if (r.actions().contains(AHCommand::Complete)) {
				if (r.defaultAction() == AHCommand::Complete) {
					connect(pb_complete_,SIGNAL(clicked()),SLOT(doExecute()));
					pb_complete_->setDefault(true);
					pb_complete_->setFocus();
				}
				else {
					connect(pb_complete_,SIGNAL(clicked()),SLOT(doComplete()));
				}
				pb_complete_->setEnabled(true);
			}
			else {
				pb_complete_->setEnabled(false);
			}
		}

		pb_cancel_ = ui_.buttonBox->addButton(QDialogButtonBox::Cancel);
		connect(pb_cancel_, SIGNAL(clicked()),SLOT(doCancel()));
	}
	else {
		pb_complete_ = ui_.buttonBox->addButton(QDialogButtonBox::Ok);
		connect(pb_complete_,SIGNAL(clicked()),SLOT(close()));
	}

	if (!r.data().title().isEmpty()) {
		setWindowTitle(QString("%1 (%2)").arg(r.data().title()).arg(receiver.full()));
	}
	else {
		setWindowTitle(QString("%1").arg(receiver.full()));
	}

	adjustSize();
	show();
	setParent(0);
}

void AHCFormDlg::doPrev()
{
	ui_.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(receiver_,AHCommand(node_,data(),sessionId_,AHCommand::Prev), client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doNext()
{
	ui_.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(receiver_,AHCommand(node_,data(),sessionId_,AHCommand::Next),client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doExecute()
{
	ui_.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(receiver_,AHCommand(node_,data(),sessionId_),client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doComplete()
{
	ui_.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(receiver_,AHCommand(node_,data(),sessionId_,AHCommand::Complete), client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doCancel()
{
	ui_.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(receiver_,AHCommand(node_,sessionId_,AHCommand::Cancel), client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::commandExecuted()
{
	ui_.busy->stop();
	close();
}

XData AHCFormDlg::data() const
{
	XData x;
	x.setFields(xdata_->fields());
	x.setType(XData::Data_Submit);
	return x;
}

