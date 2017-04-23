/*
 * ahcformdlg.h - Ad-Hoc Command Form Dialog
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

#ifndef AHCFORMDLG_H
#define AHCFORMDLG_H

#include <QObject>
#include <QDialog>
#include <QString>

#include "xmpp_xdata.h"
#include "xmpp_jid.h"

class QPushButton;
class BusyWidget;
class AHCommand;
class XDataWidget;
class PsiCon;
namespace XMPP {
	class Client;
}

#include "ui_ahcformdlg.h"

class AHCFormDlg : public QDialog
{
	Q_OBJECT

public:
	AHCFormDlg(PsiCon *psi, const AHCommand& r, const XMPP::Jid& receiver, XMPP::Client* client, bool final = false);

protected:
	XMPP::XData data() const;

protected slots:
	void doPrev();
	void doNext();
	void doComplete();
	void doExecute();
	void doCancel();
	void commandExecuted();

private:
	Ui::AHCFormDlg _ui;
	PsiCon *_psi;
	QPushButton* _pb_prev;
	QPushButton* _pb_next;
	QPushButton* _pb_complete;
	QPushButton* _pb_cancel;
	XDataWidget* _xdata;

	XMPP::Jid _receiver;
	QString node_;
	XMPP::Client* _client;
	QString sessionId_;
};

#endif
