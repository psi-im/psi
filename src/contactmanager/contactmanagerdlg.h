/*
 * contactmanagerdlg.h
 * Copyright (C) 2010 Rion
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

#ifndef CONTACTMANAGERDLG_H
#define CONTACTMANAGERDLG_H
#include "ui_contactmanagerdlg.h"

#include <QDialog>
#include "contactmanagermodel.h"
class PsiAccount;

namespace Ui {
    class ContactManagerDlg;
}

class ContactManagerDlg : public QDialog {
    Q_OBJECT
    Q_DISABLE_COPY(ContactManagerDlg)
public:
	explicit ContactManagerDlg(PsiAccount *pa);
    virtual ~ContactManagerDlg();

protected:
    virtual void changeEvent(QEvent *e);

private:
	void changeDomain(QList<UserListItem *>& users);
	void changeGroup(QList<UserListItem *>& users);
	void exportRoster(QList<UserListItem *>& users);
	void importRoster();

	Ui::ContactManagerDlg ui_;
	PsiAccount *pa_;
	ContactManagerModel *um;

private slots:
	void doSelect();
	void executeCurrent();
	void showParamField(int index);
	void client_rosterUpdated(bool,int,QString);
};

#endif // CONTACTMANAGERDLG_H
