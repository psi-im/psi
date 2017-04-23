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

AHCFormDlg::AHCFormDlg(PsiCon *psi, const AHCommand& r, const Jid& receiver, XMPP::Client* client, bool final) :
	QDialog(0),
    _psi(psi),
	_pb_prev(0),
	_pb_next(0),
	_pb_complete(0),
	_pb_cancel(0),
	_xdata(0),
	_receiver(receiver),
	_client(client)
{
	_ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	// Save node
	node_ = r.node();
	sessionId_ = r.sessionId();

	_ui.lb_note->setText(r.note().text);
	_ui.lb_note->setVisible(r.hasNote());

	_ui.lb_instructions->setText(r.data().instructions());
	_ui.lb_instructions->setVisible(!r.data().instructions().isEmpty());

	// XData form
	_xdata = new XDataWidget(_psi, this, _client, receiver);
	_xdata->setForm(r.data(), false);
	_ui.scrollArea->setWidget(_xdata);
	if (!r.hasData() && (r.hasNote() || !r.data().instructions().isEmpty()))
		_ui.scrollArea->setVisible(false);

	_ui.busy->setVisible(!final);

	if (!final) {
		if (r.actions().empty()) {
			// Single stage dialog
			_pb_complete = _ui.buttonBox->addButton(tr("Finish"), QDialogButtonBox::AcceptRole);
			connect(_pb_complete,SIGNAL(clicked()),SLOT(doExecute()));
		}
		else {
			// Multi-stage dialog

			// Previous
			_pb_prev = _ui.buttonBox->addButton(tr("Previous"), QDialogButtonBox::ActionRole);
			if (r.actions().contains(AHCommand::Prev)) {
				if (r.defaultAction() == AHCommand::Prev) {
					_pb_prev->setDefault(true);
					_pb_prev->setFocus();
				}
				connect(_pb_prev,SIGNAL(clicked()),SLOT(doPrev()));
				_pb_prev->setEnabled(true);
			}
			else {
				_pb_prev->setEnabled(false);
			}

			// Next
			_pb_next = _ui.buttonBox->addButton(tr("Next"), QDialogButtonBox::ActionRole);
			if (r.actions().contains(AHCommand::Next)) {
				if (r.defaultAction() == AHCommand::Next) {
					connect(_pb_next,SIGNAL(clicked()),SLOT(doExecute()));
					_pb_next->setDefault(true);
					_pb_next->setFocus();
				}
				else {
					connect(_pb_next,SIGNAL(clicked()),SLOT(doNext()));
				}
				_pb_next->setEnabled(true);
			}
			else {
				_pb_next->setEnabled(false);
			}

			// Complete
			_pb_complete = _ui.buttonBox->addButton(tr("Finish"), QDialogButtonBox::AcceptRole);
			if (r.actions().contains(AHCommand::Complete)) {
				if (r.defaultAction() == AHCommand::Complete) {
					connect(_pb_complete,SIGNAL(clicked()),SLOT(doExecute()));
					_pb_complete->setDefault(true);
					_pb_complete->setFocus();
				}
				else {
					connect(_pb_complete,SIGNAL(clicked()),SLOT(doComplete()));
				}
				_pb_complete->setEnabled(true);
			}
			else {
				_pb_complete->setEnabled(false);
			}
		}

		_pb_cancel = _ui.buttonBox->addButton(QDialogButtonBox::Cancel);
		connect(_pb_cancel, SIGNAL(clicked()),SLOT(doCancel()));
	}
	else {
		_pb_complete = _ui.buttonBox->addButton(QDialogButtonBox::Ok);
		connect(_pb_complete,SIGNAL(clicked()),SLOT(close()));
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
	_ui.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(_receiver,AHCommand(node_,data(),sessionId_,AHCommand::Prev), _client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doNext()
{
	_ui.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(_receiver,AHCommand(node_,data(),sessionId_,AHCommand::Next),_client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doExecute()
{
	_ui.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(_receiver,AHCommand(node_,data(),sessionId_),_client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doComplete()
{
	_ui.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(_receiver,AHCommand(node_,data(),sessionId_,AHCommand::Complete), _client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::doCancel()
{
	_ui.busy->start();
	AHCExecuteTask* t = new AHCExecuteTask(_receiver,AHCommand(node_,sessionId_,AHCommand::Cancel), _client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
	t->go(true);
}

void AHCFormDlg::commandExecuted()
{
	_ui.busy->stop();
	close();
}

XData AHCFormDlg::data() const
{
	XData x;
	x.setFields(_xdata->fields());
	x.setType(XData::Data_Submit);
	return x;
}

