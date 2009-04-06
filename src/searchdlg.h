/*
 * searchdlg.h
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

#ifndef SEARCHDLG_H
#define SEARCHDLG_H

#include <QDialog>

#include "ui_search.h"

class PsiAccount;
class QString;
class QStringList;

namespace XMPP {
	class Jid;
}

using namespace XMPP;

class SearchDlg : public QDialog, public Ui::Search
{
	Q_OBJECT
public:
	SearchDlg(const XMPP::Jid &, PsiAccount *);
	~SearchDlg();

signals:
	void aInfo(const Jid &);
	void add(const XMPP::Jid &, const QString &, const QStringList &, bool authReq);

private slots:
	void doSearchGet();
	void doSearchSet();
	void selectionChanged();
	void itemActivated(QTreeWidgetItem* item, int column);
	void jt_finished();
	void doStop();
	void doAdd();
	void doInfo();

private:
	class Private;
	Private *d;

	void addEntry(const QString &jid, const QString &nick, const QString &first, const QString &last, const QString &email);
	void clear();
};

#endif
