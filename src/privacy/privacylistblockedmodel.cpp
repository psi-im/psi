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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QSortFilterProxyModel>

#include "privacylistblockedmodel.h"
#include "privacylistmodel.h"

PrivacyListBlockedModel::PrivacyListBlockedModel(QObject* parent) : QSortFilterProxyModel(parent)
{
}

bool PrivacyListBlockedModel::lessThan(const QModelIndex & left, const QModelIndex & right ) const
{
	return left.row() < right.row();
}

bool PrivacyListBlockedModel::filterAcceptsColumn(int source_column, const QModelIndex &) const
{
	return source_column == PrivacyListModel::ValueColumn;
}

bool PrivacyListBlockedModel::filterAcceptsRow(int source_row, const QModelIndex & source_parent ) const
{
	return sourceModel()->data(sourceModel()->index(source_row,0,source_parent),PrivacyListModel::BlockedRole).toBool();
}
