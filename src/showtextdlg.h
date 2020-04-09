/*
 * showtextdlg.h - dialog for displaying a text file
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef CS_SHOWTEXTDLG_H
#define CS_SHOWTEXTDLG_H

#include <QDialog>

// TODO looks like its better to not use this class at all
class ShowTextDlg : public QDialog {
    Q_OBJECT
public:
    ShowTextDlg(const QString &fname, bool rich = false, QWidget *parent = nullptr);
    ShowTextDlg(const QString &text, bool nonfile, bool rich, QWidget *parent);
};

#endif // CS_SHOWTEXTDLG_H
