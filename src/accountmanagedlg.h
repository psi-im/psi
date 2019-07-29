/*
 * accountmanagedlg.h - dialogs for manipulating PsiAccounts
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2001-2009  Justin Karneges, Michail Pishchagin
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef ACCOUNTMANAGEDLG_H
#define ACCOUNTMANAGEDLG_H

#include <QTreeWidget>

#include "ui_accountmanage.h"

namespace XMPP {
    class Jid;
    class Client;
}

class PsiCon;
class PsiAccount;
class QTreeWidgetItem;

class AccountManageTree : public QTreeWidget
{
    Q_OBJECT

public:
    AccountManageTree(QWidget *parent = nullptr);

protected:
    void dropEvent(QDropEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);

signals:
    void orderChanged(QList<PsiAccount *> accountsList);
};

class AccountManageDlg : public QDialog, public Ui::AccountManage
{
    Q_OBJECT
public:
    AccountManageDlg(PsiCon *);
    ~AccountManageDlg();

private slots:
    void qlv_selectionChanged(QTreeWidgetItem *, QTreeWidgetItem *);
    void add();
    void modify();
    void modify(QTreeWidgetItem *);
    void remove();
    void accountAdded(PsiAccount *);
    void accountRemoved(PsiAccount *);

private:
    PsiCon *psi;
    QAction* removeAction_;
};

#endif
