/*
 * mucaffiliationsview.cpp
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

#include "mucaffiliationsview.h"

#include "xmpp_jid.h"

#include <QHeaderView>

MUCAffiliationsView::MUCAffiliationsView(QWidget *parent) : QTreeView(parent)
{
    setRootIsDecorated(false);
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setItemsExpandable(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}

void MUCAffiliationsView::removeCurrent()
{
    QModelIndex index = currentIndex();
    if (index.isValid() && index.parent().isValid()) {
        model()->removeRows(index.row(), 1, index.parent());
    }
}

void MUCAffiliationsView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    // Commenting these optimizations, since they cause too much trouble
    // bool add_before = previous.isValid() && (model()->flags(previous) & Qt::ItemIsEnabled);
    // bool remove_before = previous.isValid() && previous.parent().isValid();
    bool add_after    = current.isValid() && (model()->flags(current) & Qt::ItemIsEnabled);
    bool remove_after = current.isValid() && current.parent().isValid();

    // if (add_before != add_after)
    emit addEnabled(add_after);
    // if (remove_before != remove_after)
    emit removeEnabled(remove_after);
}
