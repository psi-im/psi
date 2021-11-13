/*
 * aboutdlg.h
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include "ui_about.h"

#include <QDialog>

class AboutDlg : public QDialog {
    Q_OBJECT

public:
    AboutDlg(QWidget *parent = nullptr);

protected:
    QString loadText(const QString &fileName);
    QString details(QString name, QString email, QString xmppAddress, QString www, QString desc);

private:
    Ui::AboutDlg ui_;
};

#endif // ABOUTDLG_H
