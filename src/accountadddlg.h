/*
 * accountadddlg.h - dialogs for manipulating PsiAccounts
 * Copyright (C) 2001-2002  Justin Karneges
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

#ifndef ACCOUNTADDDLG_H
#define ACCOUNTADDDLG_H

#include "ui_accountadd.h"

#include <QDialog>

class PsiCon;
class QString;
class QWidget;

class AccountAddDlg : public QDialog, public Ui::AccountAdd {
    Q_OBJECT
public:
    AccountAddDlg(PsiCon *, QWidget *parent = nullptr);
    ~AccountAddDlg();

private slots:
    void add();
    void setAddButton(const QString &);

private:
    PsiCon *psi;
    QString createNewAccountName(QString def);
};

#endif // ACCOUNTADDDLG_H
