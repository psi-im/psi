/*
 * profiledlg.h - dialogs for manipulating profiles
 * Copyright (C) 2001-2003  Justin Karneges
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

#ifndef PROFILEDLG_H
#define PROFILEDLG_H

#include "ui_profilemanage.h"
#include "ui_profilenew.h"
#include "ui_profileopen.h"
#include "varlist.h"

#include <QDialog>

class QButtonGroup;

class ProfileOpenDlg : public QDialog, public Ui::ProfileOpen {
    Q_OBJECT
public:
    ProfileOpenDlg(const QString &, const VarList &, const QString &, QWidget *parent = nullptr);
    ~ProfileOpenDlg();

    QString newLang;

private slots:
    void manageProfiles();
    void langChange(int);

private:
    void reload(const QString &);

    VarList langs;
    int     langSel;
};

class ProfileManageDlg : public QDialog, public Ui::ProfileManage {
    Q_OBJECT
public:
    ProfileManageDlg(const QString &, QWidget *parent = nullptr);

private slots:
    void slotProfileNew();
    void slotProfileRename();
    void slotProfileDelete();
    void updateSelection();
};

class ProfileNewDlg : public QDialog, public Ui::ProfileNew {
    Q_OBJECT
public:
    ProfileNewDlg(QWidget *parent = nullptr);

    QString name;

private slots:
    void slotCreate();
    void nameModified();

private:
    QButtonGroup *buttonGroup_;
};

#endif // PROFILEDLG_H
