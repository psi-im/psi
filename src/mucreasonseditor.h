/*
 * mucreasonseditor.cpp
 * Copyright (C) 2001-2019  Psi Team
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

#ifndef MUCREASONSEDITOR_H
#define MUCREASONSEDITOR_H

#include <QDialog>

namespace Ui {
    class MUCReasonsEditor;
}

class MUCReasonsEditor: public QDialog
{
    Q_OBJECT
public:
    MUCReasonsEditor(QWidget* parent = nullptr);
    ~MUCReasonsEditor();
    QString reason() const { return reason_; }

private:
    Ui::MUCReasonsEditor *ui_;
    QString reason_;

private slots:
    void addButtonClicked();
    void removeButtonClicked();
    void save();
    void currentChanged(const QString&);

protected slots:
    void accept();
};

#endif
