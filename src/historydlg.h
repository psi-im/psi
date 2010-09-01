/*
 * historydlg.h
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#ifndef HISTORYDLG_H
#define HISTORYDLG_H

#include <QDialog>
#include "eventdb.h"
#include "ui_history.h"
class PsiAccount;
namespace XMPP {
	class Jid;
}

class HistoryDlg : public QDialog
{
	Q_OBJECT

public:
	HistoryDlg(const XMPP::Jid&, PsiAccount*);

	
private slots:
	void doSomething();
        void openSelectedContact();
        void getLatest();
        void getPrevious();
        void getNext();
        void findMessages();
        void edb_finished();
        void changeAccount(const QString accountName);
	
private:
	Ui::HistoryDlg ui_;
        void setButtons();
        void ReadMessages();
        void displayResult(const EDBResult *, int, int max=-1);
        void highlightBlocks(const QString text);
        void listAccounts();
        class Private;
        Private *d;
};

#endif
