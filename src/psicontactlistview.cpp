/*
 * psicontactlistview.cpp - Psi-specific ContactListView-subclass
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psicontactlistview.h"

#include <QHelpEvent>

#include "psicontactlistviewdelegate.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "contactlistmodel.h"

PsiContactListView::PsiContactListView(QWidget* parent)
	: ContactListDragView(parent)
{
	setIndentation(4);
	setItemDelegate(new PsiContactListViewDelegate(this));
}

PsiContactListViewDelegate* PsiContactListView::itemDelegate() const
{
	return static_cast<PsiContactListViewDelegate*>(ContactListDragView::itemDelegate());
}

void PsiContactListView::showToolTip(const QModelIndex& index, const QPoint& globalPos) const
{
	Q_UNUSED(globalPos);
	QString text = index.data(Qt::ToolTipRole).toString();
	PsiToolTip::showText(globalPos, text, this);
}

void PsiContactListView::setModel(QAbstractItemModel* model)
{
	ContactListDragView::setModel(model);
	QAbstractItemModel* connectToModel = realModel();
	if (dynamic_cast<ContactListModel*>(connectToModel)) {
		connect(connectToModel, SIGNAL(contactAlert(const QModelIndex&)), SLOT(contactAlert(const QModelIndex&)));
	}
}

void PsiContactListView::contactAlert(const QModelIndex& realIndex)
{
	QModelIndex index = proxyIndex(realIndex);
	itemDelegate()->contactAlert(index);

	bool alerting = index.data(ContactListModel::IsAlertingRole).toBool();
	if (alerting && PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool()) {
		ensureVisible(index);
	}
}

void PsiContactListView::doItemsLayoutStart()
{
	ContactListDragView::doItemsLayoutStart();
	itemDelegate()->clearAlerts();
}
