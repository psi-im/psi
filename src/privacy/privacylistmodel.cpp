/*
 * privacylistmodel.cpp
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

#include "privacylistmodel.h"

#include "privacylist.h"
#include "privacyruledlg.h"

#include <QAbstractListModel>

PrivacyListModel::PrivacyListModel(const PrivacyList &list, QObject *parent) : QAbstractListModel(parent), list_(list)
{
}

int PrivacyListModel::rowCount(const QModelIndex &) const { return list_.items().count(); }

int PrivacyListModel::columnCount(const QModelIndex &) const { return 2; }

QVariant PrivacyListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= list_.items().count())
        return QVariant();

    if (role == Qt::DisplayRole) {
        if (index.column() == TextColumn)
            return list_.item(index.row()).toString();
        else if (index.column() == ValueColumn)
            return list_.item(index.row()).value();
    } else if (role == BlockedRole) {
        return list_.item(index.row()).isBlock();
    }

    return QVariant();
}

void PrivacyListModel::setList(const PrivacyList &list)
{
    beginResetModel();
    list_ = list;
    endResetModel();
}

bool PrivacyListModel::moveUp(const QModelIndex &index)
{
    beginResetModel();
    bool moved = index.isValid() && list_.moveItemUp(index.row());
    endResetModel();
    return moved;
}

bool PrivacyListModel::moveDown(const QModelIndex &index)
{
    beginResetModel();
    bool moved = index.isValid() && list_.moveItemDown(index.row());
    endResetModel();
    return moved;
}

bool PrivacyListModel::removeRows(int row, int count, const QModelIndex &)
{
    // qDebug("PrivacyListModel::removeRows");
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    while (count > 0) {
        list_.removeItem(row);
        count--;
    }
    endRemoveRows();
    return true;
}

bool PrivacyListModel::add()
{
    PrivacyRuleDlg d;
    if (d.exec() == QDialog::Accepted) {
        beginResetModel();
        list_.insertItem(0, d.rule());
        endResetModel();
        return true;
    }
    return false;
}

void PrivacyListModel::insertItem(int pos, const PrivacyListItem &item)
{
    beginResetModel();
    list_.insertItem(pos, item);
    endResetModel();
}

bool PrivacyListModel::edit(const QModelIndex &index)
{
    if (index.isValid()) {
        PrivacyRuleDlg d;
        d.setRule(list_.item(index.row()));
        if (d.exec() == QDialog::Accepted) {
            list_.updateItem(index.row(), d.rule());
            emit dataChanged(index, index);
            return true;
        }
    }
    return false;
}
