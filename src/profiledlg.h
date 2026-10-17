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

#include "ui_profileopen.h"
#include "varlist.h"

#include <QDialog>

class QListWidgetItem;

class ProfileOpenDlg : public QDialog, public Ui::ProfileOpen {
    Q_OBJECT
public:
    ProfileOpenDlg(const QString &preferred, const VarList &langs, const QString &curLang,
                   QWidget *parent = nullptr);

    QString selectedProfile() const;
    QString preferredProfile() const;
    QString selectedLanguage() const;

private:
    void reload(const QString &preferred);
    void addProfile();
    void updateAddButton();
    void updateSelection();
    void profileRenamed(QListWidgetItem *item);
    void confirmDelete(const QString &name);
    void deleteProfile(const QString &name);

    QString _preferredProfile;
};

#endif // PROFILEDLG_H
