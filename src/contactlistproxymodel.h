/*
 * contactlistproxymodel.h - contact list model sorting and filtering
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef CONTACTLISTPROXYMODEL_H
#define CONTACTLISTPROXYMODEL_H

#include <QSortFilterProxyModel>

class PsiContactList;

class ContactListProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	ContactListProxyModel(QObject* parent);

	void setSourceModel(QAbstractItemModel* model);

public slots:
	void updateSorting();

signals:
	void recalculateSize();

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

	bool showOffline() const;
	bool showSelf() const;
	bool showTransports() const;
	bool showHidden() const;

private slots:
	void filterParametersChanged();
};

#endif
