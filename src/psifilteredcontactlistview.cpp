/*
 * psifilteredcontactlistview.cpp
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
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

#include "psifilteredcontactlistview.h"

#include <QKeyEvent>
#include <QPainter>

#include "psicontactlistviewdelegate.h"

PsiFilteredContactListView::PsiFilteredContactListView(QWidget* parent)
	: PsiContactListView(parent)
{
	// setFocusPolicy(Qt::NoFocus);

	// setDragEnabled(false);
	setAcceptDrops(false);
}

bool PsiFilteredContactListView::handleKeyPressEvent(QKeyEvent* e)
{
	updateKeyboardModifiers(e);

	switch (e->key()) {
	case Qt::Key_Enter:
	case Qt::Key_Return: {
		if (state() == EditingState)
			return false;

		QModelIndex currentIndex = this->currentIndex();
		if (currentIndex.isValid()) {
			// TODO: probably should select the item from the filteredListView_
			// in the contactListView_ as well
			activate(currentIndex);
		}
		return true;
	}
	case Qt::Key_Home: {
		selectIndex(0);
		return true;
	}
	case Qt::Key_End: {
		selectIndex(model()->rowCount()-1);
		return true;
	}
	case Qt::Key_Up:
	case Qt::Key_Down: {
		moveSelection(1, e->key() == Qt::Key_Up ? Backward : Forward);
		return true;
	}
	case Qt::Key_PageUp:
	case Qt::Key_PageDown: {
		int delta = 0;
		QModelIndex index = model()->index(0, 0, QModelIndex());
		if (index.isValid()) {
			int itemHeight = itemDelegate()->sizeHint(QStyleOptionViewItem(), index).height();
			if (itemHeight)
				delta = viewport()->height() / itemHeight;
		}
		moveSelection(delta, e->key() == Qt::Key_PageUp ? Backward : Forward);
		return true;
	}
	default:
		;
	}

	return false;
}

void PsiFilteredContactListView::moveSelection(uint delta, PsiFilteredContactListView::Direction direction)
{
	QModelIndex currentIndex = this->currentIndex();
	int row = currentIndex.row();
	if (currentIndex.isValid())
		row = direction == Forward ? (row + delta) : (row - delta);
	else
		row = 0;
	selectIndex(row);
}

void PsiFilteredContactListView::selectIndex(int row)
{
	setUpdatesEnabled(false);

	row = qMax(0, qMin(model()->rowCount()-1, row));
	QModelIndex newIndex = model()->index(row, 0, QModelIndex());
	if (newIndex.isValid()) {
		QItemSelection selection(selectionModel()->selection());
		if (extendSelection()) {
			selection.select(currentIndex(), newIndex);
			selection.select(newIndex, currentIndex());
		}

		setCurrentIndex(newIndex);

		if (extendSelection()) {
			selectionModel()->select(selection, QItemSelectionModel::Select);
		}
	}

	setUpdatesEnabled(true);
}

void PsiFilteredContactListView::itemActivated(const QModelIndex& index)
{
	emit quitFilteringMode();
	PsiContactListView::itemActivated(index);
}

bool PsiFilteredContactListView::extendSelection() const
{
	return keyboardModifiers() & Qt::ShiftModifier;
}
