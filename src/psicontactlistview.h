/*
 * psicontactlistview.h - Psi-specific ContactListView-subclass
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSICONTACTLISTVIEW_H
#define PSICONTACTLISTVIEW_H

#include "contactlistdragview.h"

class ContactListViewDelegate;
class QAbstractItemModel;

class PsiContactListView : public ContactListDragView
{
    Q_OBJECT

public:
    PsiContactListView(QWidget* parent);

    // reimplemented
    void setModel(QAbstractItemModel* model);

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    void setAutoResizeEnabled(bool enabled);

protected slots:
    void alertContacts(const QModelIndexList &indexes);
    void animateContacts(const QModelIndexList &indexes, bool started);
    void optionChanged(const QString &option);

protected:
    // reimplemented
    void showToolTip(const QModelIndex& index, const QPoint& globalPos) const;
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void dragMoveEvent(QDragMoveEvent *e);

    ContactListViewDelegate *itemDelegate() const;

private:
    bool acceptableDragOperation(QDropEvent *e);

private:
    class Private;
    Private* d;
};

#endif // PSICONTACTLISTVIEW_H
