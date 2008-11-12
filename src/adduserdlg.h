/*
 * adduserdlg.h - dialog for adding contacts
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

#ifndef ADDUSERDLG_H
#define ADDUSERDLG_H

#include "ui_adduser.h"

class QString;
class QStringList;
class PsiAccount;
namespace XMPP {
	class Jid;
}

class AddUserDlg : public QDialog, public Ui::AddUser
{
	Q_OBJECT
public:
	AddUserDlg(const QStringList &services, const QStringList &names, const QStringList &groups, PsiAccount *);
	AddUserDlg(const XMPP::Jid &jid, const QString &nick, const QString &group, const QStringList &groups, PsiAccount *);
	~AddUserDlg();

signals:
	void add(const XMPP::Jid &, const QString &, const QStringList &, bool authReq);

private slots:
	void ok();
	void cancel();
	void serviceActivated(int);
	void getTransID();
	void jt_getFinished();
	void jt_setFinished();
	void le_transPromptChanged(const QString &);

	void getVCardActivated();
	void resolveNickActivated();
	void resolveNickFinished();

	void jid_Changed();

private:
	void init(const QStringList &groups, PsiAccount *);
	class Private;
	Private *d;

	XMPP::Jid jid() const;
	void errorGateway(const QString &str, const QString &err);
};

#endif
