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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#ifndef PRIVACYLISTBLOCKEDMODEL
#define PRIVACYLISTBLOCKEDMODEL

#include <QSortFilterProxyModel>

class PrivacyListBlockedModel : public QSortFilterProxyModel
{
public:
	PrivacyListBlockedModel(QObject* parent = NULL);

	bool lessThan(const QModelIndex & left, const QModelIndex & right ) const;
	bool filterAcceptsColumn(int source_column, const QModelIndex & source_parent ) const;
	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent ) const;
};

#endif
