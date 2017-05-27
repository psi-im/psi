/*
 * contactlistviewdelegate.h - base class for painting contact list items
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

#pragma once

#include "hoverabletreeview.h"
#include "xmpp_status.h"

#include <QItemDelegate>

class ContactListView;
class PsiContact;
class PsiAccount;

class ContactListViewDelegate : public QItemDelegate
{
	Q_OBJECT

public:
	ContactListViewDelegate(ContactListView *parent);
	virtual ~ContactListViewDelegate();

	void recomputeGeometry();
	int avatarSize() const;

	void contactAlert(const QModelIndex &index);
	void animateContacts(const QModelIndexList &indexes, bool started);
	void clearAlerts();

	// reimplemented
	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	void setEditorData(QWidget *editor, const QModelIndex &index) const override;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
	void geometryUpdated();

protected:
	// reimplemented
	bool eventFilter(QObject *object, QEvent *event) override;

private:
	class Private;
	Private* d;
};
