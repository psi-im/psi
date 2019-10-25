/*
 * passdialog.cpp
 * Copyright (C) 2009-2010  Virnik
 * Copyright (C) 2011  Evgeny Khryukin
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

#ifndef PASSDIALOG_H
#define PASSDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;

class PassDialog : public QDialog {
    Q_OBJECT
public:
    PassDialog(const QString &jid = "", QWidget *parent = nullptr);

    QString password() const;
    bool    savePassword() const;
    void    setSavePassword(bool save);

private:
    QCheckBox *cb_savePassword;
    QLineEdit *le_password;
};

#endif // PASSDIALOG_H
