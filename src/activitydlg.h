/*
 * activitydlg.h
 * Copyright (C) 2008  Armando Jagucki
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

#ifndef ACTIVITYDLG_H
#define ACTIVITYDLG_H

#include "ui_activity.h"

#include <QDialog>

class PsiAccount;

class ActivityDlg : public QDialog {
    Q_OBJECT

public:
    ActivityDlg(QList<PsiAccount *>);

protected slots:
    void setActivity();
    void loadSpecificActivities(const QString &);

private:
    Ui::Activity        ui_;
    QList<PsiAccount *> pa_;
};

#endif // ACTIVITYDLG_H
