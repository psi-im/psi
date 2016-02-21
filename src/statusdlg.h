/*
 * statusdlg.h - dialogs for setting and reading status messages
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

#ifndef STATUSDLG_H
#define STATUSDLG_H

#include <QDialog>

#include <QList>
#include "xmpp_jid.h"

class PsiCon;
class PsiAccount;
class UserListItem;
class QKeyEvent;
namespace XMPP {
	class Status;
}

using namespace XMPP;

enum setStatusEnum{setStatusForAccount = 0, setStatusForJid, setStatusForJidList};

class StatusShowDlg : public QDialog
{
	Q_OBJECT
public:
	StatusShowDlg(const UserListItem &);
};

class StatusSetDlg : public QDialog
{
	Q_OBJECT
public:
	StatusSetDlg(PsiCon *, const Status &, bool withPriority = true);
	StatusSetDlg(PsiAccount *, const Status &, bool withPriority = true);
	~StatusSetDlg();

	void setJid(const Jid &);
	void setJidList(const QList<XMPP::Jid> &);

signals:
	void set(const XMPP::Status &, bool withPriority, bool isManualStatus);
	void cancelled();
	void setJid(const Jid &, const Status &);
	void setJidList(const QList<XMPP::Jid> &, const Status &);

private slots:
	void doButton();
	void cancel();
	void reject();
	void chooseStatusPreset(int);

private:
	class Private;
	Private *d;

	void init();
};

#endif
