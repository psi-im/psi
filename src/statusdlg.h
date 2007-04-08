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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef STATUSDLG_H
#define STATUSDLG_H

#include <qdialog.h>
#include <QKeyEvent>

class PsiCon;
class PsiAccount;
class UserListItem;
namespace XMPP {
	class Status;
}

using namespace XMPP;

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
	StatusSetDlg(PsiCon *, const Status &);
	StatusSetDlg(PsiAccount *, const Status &);
	~StatusSetDlg();

signals:
	void set(const XMPP::Status &, bool withPriority);
	void cancelled();

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
