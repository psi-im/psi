/*
 * mooddlg.h
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

#ifndef MOODDLG_H
#define MOODDLG_H

#include "ui_mood.h"

#include <QDialog>

class PsiAccount;

class MoodDlg : public QDialog {
    Q_OBJECT

public:
    MoodDlg(QList<PsiAccount *>);

protected slots:
    void setMood();

private:
    Ui::Mood            ui_;
    QList<PsiAccount *> pa_;
};

#endif // MOODDLG_H
