/*
 * contactmanagerdlg.h
 * Copyright (C) 2010  Sergey Ilinykh
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

#ifndef CONTACTMANAGERDLG_H
#define CONTACTMANAGERDLG_H

#include "contactmanagermodel.h"
#include "ui_contactmanagerdlg.h"

#include <QDialog>

class PsiAccount;

namespace Ui {
class ContactManagerDlg;
}

class ContactManagerDlg : public QDialog {
    Q_OBJECT
    Q_DISABLE_COPY(ContactManagerDlg)
public:
    explicit ContactManagerDlg(PsiAccount *pa);
    ~ContactManagerDlg() override;

protected:
    void changeEvent(QEvent *e) override;

private:
    void changeDomain(QList<UserListItem *> &users);
    void changeGroup(QList<UserListItem *> &users);
    void exportRoster(QList<UserListItem *> &users);
    void importRoster();

    Ui::ContactManagerDlg ui_;
    PsiAccount *          pa_;
    ContactManagerModel * um;

private slots:
    void doSelect();
    void executeCurrent();
    void showParamField(int index);
    void client_rosterUpdated(bool, int, QString);
};

#endif // CONTACTMANAGERDLG_H
