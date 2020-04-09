/*
 * passphrasedlg.h - class to handle entering of OpenPGP passphrase
 * Copyright (C) 2003  Justin Karneges
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

#ifndef PASSPHRASEDLG_H
#define PASSPHRASEDLG_H

#include "ui_passphrase.h"

class PassphraseDlg : public QDialog {
    Q_OBJECT
public:
    PassphraseDlg(QWidget *parent = nullptr);

    void promptPassphrase(const QString &name);

    QString getPassphrase() const;
    bool    rememberPassPhrase() const;

private:
    Ui::Passphrase ui_;
};

#endif // PASSPHRASEDLG_H
