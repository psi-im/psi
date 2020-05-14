/*
 * pgpkeydlg.h
 * Copyright (C) 2001-2009  Justin Karneges, Michail Pishchagin
 * Copyright (C) 2020  Boris Pek <tehnick-8@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#ifndef PGPKEYDLG_H
#define PGPKEYDLG_H

#include "ui_pgpkey.h"

#include <QStringList>

class QSortFilterProxyModel;
class QStandardItemModel;

class PGPKeyDlg : public QDialog {
    Q_OBJECT

public:
    enum Type { Public, Secret, All };

    PGPKeyDlg(Type, const QString &defaultKeyID, QWidget *parent = nullptr);
    const QString &keyId() const;

private slots:
    void doubleClicked(const QModelIndex &index);
    void filterTextChanged();
    void do_accept();
    void show_info();

protected:
    // reimplemented
    bool eventFilter(QObject *watched, QEvent *event);

private:
    Ui::PGPKey             m_ui;
    QString                m_keyId;
    QPushButton *          m_pb_dtext;
    QStandardItemModel *   m_model;
    QSortFilterProxyModel *m_proxy;
};

#endif // PGPKEYDLG_H
