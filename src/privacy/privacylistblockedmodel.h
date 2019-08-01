/*
 * privacylistblockedmodel.cpp
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

#ifndef PRIVACYLISTBLOCKEDMODEL_H
#define PRIVACYLISTBLOCKEDMODEL_H

#include <QSortFilterProxyModel>

class PrivacyListBlockedModel : public QSortFilterProxyModel
{
public:
    PrivacyListBlockedModel(QObject* parent = nullptr);

    bool lessThan(const QModelIndex & left, const QModelIndex & right ) const;
    bool filterAcceptsColumn(int source_column, const QModelIndex & source_parent ) const;
    bool filterAcceptsRow(int source_row, const QModelIndex & source_parent ) const;
};

#endif // PRIVACYLISTBLOCKEDMODEL_H
