/*
 * mucjoindlg.h
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

#ifndef MUCJOINDLG_H
#define MUCJOINDLG_H

#include <QDialog>

#include "ui_mucjoin.h"

class PsiCon;
class QString;
class PsiAccount;

#include "xmpp_jid.h"

class MUCJoinDlg : public QDialog
{
	Q_OBJECT

public:

	enum MucJoinReason {
		MucAutoJoin,
		MucCustomJoin
	};

	MUCJoinDlg(PsiCon *, PsiAccount *);
	~MUCJoinDlg();

	void setJid(const XMPP::Jid& jid);
	void setNick(const QString& nick);
	void setPassword(const QString& password);

	void joined();
	void error(int, const QString &);

public slots:
	void done(int);
	void doJoin(MucJoinReason reason = MucCustomJoin);

	// reimplemented
	void accept();

public:
	MucJoinReason getReason() const { return reason_; };


private slots:
	void updateIdentity(PsiAccount *);
	void updateIdentityVisibility();
	void pa_disconnected();
	void recent_activated(int);

private:
	Ui::MUCJoin ui_;
	PsiCon* controller_;
	PsiAccount* account_;
	QPushButton* joinButton_;
	XMPP::Jid jid_;
	MucJoinReason reason_;
	bool nickAlreadyCompleted_;

	void disableWidgets();
	void enableWidgets();
	void setWidgetsEnabled(bool enabled);
};

#endif
