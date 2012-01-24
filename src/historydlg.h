/*
 * historydlg.h
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Evgeny Khryukin
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

#ifndef HISTORYDLG_H
#define HISTORYDLG_H

#include "eventdb.h"
#include "ui_history.h"
#include "advwidget.h"

class PsiAccount;
class PsiContact;
class UserListItem;

namespace XMPP {
	class Jid;
}

class HistoryDlg : public AdvancedWidget<QDialog>
{
	Q_OBJECT

public:
	HistoryDlg(const XMPP::Jid&, PsiAccount*);
	virtual ~HistoryDlg();

	enum RequestType {
		TypeNone = 0,
		TypeNext,
		TypePrevious,
		TypeLatest,
		TypeEarliest,
		TypeFind,
		TypeFindOldest,
		TypeDate
	};

private slots:
	void openSelectedContact();
	void getLatest();
	void getEarliest();
	void getPrevious();
	void getNext();
	void getDate();
	void refresh();
	void findMessages();
	void edb_finished();
	void highlightBlocks(const QString text);
	void changeAccount(const QString accountName);
	void removeHistory();
	void exportHistory();
	void openChat();
	void doMenu();
	void removedContact(PsiContact*);

protected:
	bool eventFilter(QObject *, QEvent *);

private:
	void setButtons();
	void setButtons(bool act);
	void loadContacts();
	void displayResult(const EDBResult , int, int max=-1);
	QFont fontForOption(const QString& option);
	void listAccounts();
	UserListItem* currentUserListItem();
	void startRequest();
	void stopRequest();

	EDBHandle* getEDBHandle();

	class Private;
	Private *d;
	Ui::HistoryDlg ui_;
	QStringList jids_;
};

#endif
