/*
 * privacydlg.h
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2006  Remko Troncon
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

#ifndef PRIVACYDLG_H
#define PRIVACYDLG_H

#include <QDialog>
#include <QPointer>

#include "privacylistmodel.h"
#include "ui_privacy.h"

class QWidget;
class QString;
class QStringList;
class PrivacyManager;

class PrivacyDlg : public QDialog
{
    Q_OBJECT

public:
    PrivacyDlg(const QString&, PrivacyManager* manager, QWidget* parent = nullptr);
    ~PrivacyDlg() { };

protected:
    void rememberSettings();
    void revertSettings();
    void listChanged();

protected slots:
    void setWidgetsEnabled(bool);
    void setEditRuleEnabled(bool);
    void updateLists(const QString&, const QString&, const QStringList&);
    void refreshList(const PrivacyList&);
    void active_selected(int);
    void default_selected(int);
    void list_selected(int i);
    void list_changed(int);
    void list_failed();
    void changeList_succeeded(QString);
    void changeList_failed();
    void change_succeeded();
    void changeActiveList_succeeded(QString);
    void changeDefaultList_succeeded(QString);
    void change_failed();
    void close();
    void addRule();
    void editCurrentRule();
    void removeCurrentRule();
    void moveCurrentRuleUp();
    void moveCurrentRuleDown();
    void applyList();
    void newList();
    void removeList();
    void renameList();

private:
    Ui::Privacy ui_;
    int previousActive_, previousDefault_, previousList_;
    QPointer<PrivacyManager> manager_;
    PrivacyListModel model_;
    bool newList_;
};

#endif
