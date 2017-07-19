/*
 * contactlistview.cpp - base contact list widget class
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

#include <QTreeView>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QPainter>
#include <QLinearGradient>
#include <QScrollBar>
#include <QCoreApplication>

#include "contactlistitem.h"
#include "contactlistitemmenu.h"
#include "contactlistmodel.h"
#include "contactlistview.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "contactlistproxymodel.h"
#include "debug.h"

ContactListView::ContactListView(QWidget* parent)
	: HoverableTreeView(parent)
	, contextMenuActive_(false)
{
	setUniformRowHeights(false);
	setAlternatingRowColors(true);
	setRootIsDecorated(false);
	// on Macs Enter key is the default EditKey, so we can't use EditKeyPressed
	setEditTriggers(QAbstractItemView::EditKeyPressed); // lesser evil, otherwise no editing will be possible at all due to Qt bug
	// setEditTriggers(QAbstractItemView::NoEditTriggers);
	setIndentation(5);
	header()->hide();
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#if QT_VERSION < QT_VERSION_CHECK(5,7,0)
	verticalScrollBar()->setSingleStep(1); // makes scrolling really slow with qt>=5.7. w/o it Qt has some adaptive algo
#endif
	// setItemDelegate(new PsiContactListViewDelegate(this));

#ifdef Q_OS_MAC
	setFrameShape(QFrame::NoFrame);
#endif

	connect(this, SIGNAL(doubleClicked(const QModelIndex&)), SLOT(itemActivated(const QModelIndex&)));
	// showStatus_ = PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.show").toBool();
}

static void setExpandedState(QTreeView *view, QAbstractItemModel *model, const QModelIndex &parent)
{
	Q_ASSERT(model);

	for (int i = 0; i < model->rowCount(parent); i++) {
		QModelIndex index = model->index(i, 0, parent);
		view->setExpanded(index, model->data(index, ContactListModel::ExpandedRole).toBool());
		if (model->hasChildren(index)) {
			setExpandedState(view, model, index);
		}
	}
}

//void ContactListView::doItemsLayout()
//{
//	if (!model())
//		return;
//	HoverableTreeView::doItemsLayout();
//	updateGroupExpandedState();
//}

void ContactListView::updateGroupExpandedState()
{
	if (!model())
		return;

	setExpandedState(this, model(), QModelIndex());
}

void ContactListView::showOfflineChanged()
{
	updateGroupExpandedState();
}

bool ContactListView::isContextMenuVisible()
{
	return contextMenu_ && contextMenuActive_;
}

void ContactListView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	HoverableTreeView::selectionChanged(selected, deselected);

	updateContextMenu();
}

void ContactListView::updateContextMenu()
{
	if (isContextMenuVisible())
		return;

	delete contextMenu_;
	contextMenu_ = 0;

	// FIXME: need to implement context menu merging
	if (selectedIndexes().count() == 1) {
		QModelIndex index = realIndex(selectedIndexes().first());
		ContactListItem *item = realModel()->toItem(index);
		if (item) {
			contextMenu_ = createContextMenuFor(item);
			addContextMenuActions();
		}
	}
}

ContactListItemMenu* ContactListView::createContextMenuFor(ContactListItem* item) const
{
	if (item)
		return item->contextMenu();

	return nullptr;
}

void ContactListView::focusInEvent(QFocusEvent* event)
{
	HoverableTreeView::focusInEvent(event);
	addContextMenuActions();
}

void ContactListView::focusOutEvent(QFocusEvent* event)
{
	HoverableTreeView::focusOutEvent(event);
	removeContextMenuActions();
}

void ContactListView::addContextMenuActions()
{
	if (contextMenu_) {
		for (QAction* action: contextMenu_->availableActions()) {
			addContextMenuAction(action);
		}
	}
}

void ContactListView::addContextMenuAction(QAction* action)
{
	addAction(action);
}

void ContactListView::removeContextMenuActions()
{
	if (contextMenu_)
		foreach(QAction* action, contextMenu_->availableActions())
			removeAction(action);
}

void ContactListView::contextMenuEvent(QContextMenuEvent* e)
{
	if (contextMenu_ && contextMenu_->availableActions().count() > 0) {
		e->accept();
		contextMenuActive_ = true;
		contextMenu_->exec(e->globalPos());
		contextMenuActive_ = false;
	}
	else {
		e->ignore();
	}
}

void ContactListView::setModel(QAbstractItemModel* model)
{
	HoverableTreeView::setModel(model);
	QAbstractItemModel* connectToModel = realModel();
	if (qobject_cast<ContactListModel*>(connectToModel)) {
		connect(this, SIGNAL(expanded(const QModelIndex&)), SLOT(itemExpanded(const QModelIndex&)));
		connect(this, SIGNAL(collapsed(const QModelIndex&)), SLOT(itemCollapsed(const QModelIndex&)));
		connect(this, SIGNAL(realExpanded(const QModelIndex&)), connectToModel, SLOT(expanded(const QModelIndex&)));
		connect(this, SIGNAL(realCollapsed(const QModelIndex&)), connectToModel, SLOT(collapsed(const QModelIndex&)));
		connect(connectToModel, SIGNAL(inPlaceRename()), SLOT(rename()));

		// connection to showOfflineChanged() should be established after the proxy model does so,
		// otherwise we won't get consistently re-expanded groups. Qt::QueuedConnection could
		// be advised for in such case.
		// connect(connectToModel, SIGNAL(showOfflineChanged()), SLOT(showOfflineChanged()));
		connect(model, SIGNAL(layoutChanged()), SLOT(showOfflineChanged()));
	}
}

void ContactListView::resizeEvent(QResizeEvent* e)
{
	HoverableTreeView::resizeEvent(e);
	if (header()->count() > 0)
		header()->resizeSection(0, viewport()->width());
}

void ContactListView::rowsInserted(const QModelIndex &parent, int start, int end)
{
	HoverableTreeView::rowsInserted(parent, start, end);
	for (int i = start; i <= end; ++i) {
		QModelIndex index = parent.child(i, 0);
		if (realIndex(index).data(ContactListModel::ExpandedRole).toBool())
			setExpanded(index, true);
	}
}

void ContactListView::itemExpanded(const QModelIndex& index)
{
	emit realExpanded(realIndex(index));
}

void ContactListView::itemCollapsed(const QModelIndex& index)
{
	emit realCollapsed(realIndex(index));
}

/**
 * Branches? We don't want no steenking branches!
 */
void ContactListView::drawBranches(QPainter*, const QRect&, const QModelIndex&) const
{
}

void ContactListView::toggleExpandedState(const QModelIndex& index)
{
	setExpanded(index, !index.data(ContactListModel::ExpandedRole).toBool());
}

/**
 * Make Enter/Return/F2 to not trigger editing, and make Enter/Return call activated().
 */
void ContactListView::keyPressEvent(QKeyEvent* event)
{
	switch (event->key()) {
	case Qt::Key_F2:
		event->ignore();
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		if (state() != EditingState || hasFocus()) {
			if (currentIndex().isValid())
				emit doubleClicked(currentIndex());
		}
		else {
			event->ignore();
		}
		break;
	case Qt::Key_Space:
		if (state() != EditingState) {

			ContactListItem *item = qvariant_cast<ContactListItem*>(currentIndex().data(ContactListModel::ContactListItemRole));

			if (item->isGroup()) {
				toggleExpandedState(currentIndex());
			}
			else {
				QContextMenuEvent e(QContextMenuEvent::Keyboard, visualRect(currentIndex()).center());
				QCoreApplication::sendEvent(this, &e);
			}
		}
		else {
			event->ignore();
		}
	break;
	default:
		HoverableTreeView::keyPressEvent(event);
	}
}

void ContactListView::rename()
{
	if (!hasFocus())
		return;

	QModelIndexList indexes = selectedIndexes();
	if (!indexes.count()) {
		return;
	}
	if (indexes.count() > 1) {
		qWarning("ContactListView::rename(): selectedIndexes().count() > 1");
		return;
	}

	QModelIndex indexToEdit = indexes.first();
	QModelIndex parent = indexToEdit.parent();
	while (parent.isValid()) {
		expand(parent);
		parent = parent.parent();
	}

	scrollTo(indexToEdit);
	edit(indexToEdit);
}

/**
 * Re-implemented in order to use PsiToolTip class to display tooltips.
 */
bool ContactListView::viewportEvent(QEvent* event)
{
	if (event->type() == QEvent::ToolTip &&
	    (isActiveWindow() || window()->testAttribute(Qt::WA_AlwaysShowToolTips)))
	{
		QHelpEvent* he = static_cast<QHelpEvent*>(event);
		showToolTip(indexAt(he->pos()), he->globalPos());
		return true;
	}

	return HoverableTreeView::viewportEvent(event);
}

void ContactListView::showToolTip(const QModelIndex& index, const QPoint& globalPos) const
{
	if (!model())
		return;
	QVariant toolTip = model()->data(index, Qt::ToolTipRole);
	PsiToolTip::showText(globalPos, toolTip.toString(), this);
}

void ContactListView::activate(const QModelIndex& index)
{
	itemActivated(index);
}

void ContactListView::itemActivated(const QModelIndex& index)
{
	model()->setData(index, QVariant(true), ContactListModel::ActivateRole);
}

static QAbstractItemModel* realModel(QAbstractItemModel* model)
{
	QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(model);
	if (proxyModel)
		return realModel(proxyModel->sourceModel());
	return model;
}

ContactListModel *ContactListView::realModel() const
{
	return qobject_cast<ContactListModel*>(::realModel(model()));
}

QModelIndexList ContactListView::realIndexes(const QModelIndexList& indexes) const
{
	QModelIndexList result;
	foreach(QModelIndex index, indexes) {
		QModelIndex ri = realIndex(index);
		if (ri.isValid())
			result << ri;
	}
	return result;
}

static QModelIndex realIndex(QAbstractItemModel* model, QModelIndex index)
{
	QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(model);
	if (proxyModel)
		return realIndex(proxyModel->sourceModel(), proxyModel->mapToSource(index));
	return index;
}

QModelIndex ContactListView::realIndex(const QModelIndex& index) const
{
	return ::realIndex(model(), index);
}

QModelIndexList ContactListView::proxyIndexes(const QModelIndexList& indexes) const
{
	QModelIndexList result;
	foreach(QModelIndex index, indexes) {
		QModelIndex pi = proxyIndex(index);
		if (pi.isValid())
			result << pi;
	}
	return result;
}

static QModelIndex proxyIndex(QAbstractItemModel* model, QModelIndex index)
{
	QSortFilterProxyModel* proxyModel = dynamic_cast<QSortFilterProxyModel*>(model);
	if (proxyModel)
		return proxyIndex(proxyModel->sourceModel(), proxyModel->mapFromSource(index));
	return index;
}

QModelIndex ContactListView::proxyIndex(const QModelIndex &index) const
{
	return ::proxyIndex(model(), index);
}

ContactListItem *ContactListView::itemProxy(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;
	return static_cast<ContactListItem*>(realIndex(index).internalPointer());
}

QLineEdit* ContactListView::currentEditor() const
{
	QWidget* w = indexWidget(currentIndex());
	return dynamic_cast<QLineEdit*>(w);
}

void ContactListView::setEditingIndex(const QModelIndex& index, bool editing) const
{
	ContactListItem *item = itemProxy(index);
	if (item) {
		item->setEditing(editing);
	}
}

void ContactListView::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
	setEditingIndex(currentIndex(), false);

	HoverableTreeView::closeEditor(editor, hint);
}

void ContactListView::ensureVisible(const QModelIndex& index)
{
	scrollTo(index, QAbstractItemView::EnsureVisible);
}
