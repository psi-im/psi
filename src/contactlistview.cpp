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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "contactlistitemproxy.h"
#include "contactlistmodel.h"
#include "contactlistview.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "contactlistgroupcache.h"
#include "contactlistgroup.h"
#include "contactlistproxymodel.h"
#ifdef YAPSI
#include "smoothscrollbar.h"
#include "yawindowtheme.h"
#include "yavisualutil.h"
#endif

#ifdef YAPSI
class ContactListViewCorner : public QWidget
{
public:
	ContactListViewCorner()
		 : QWidget(0)
	{}

protected:
	// reimplemented
	void paintEvent(QPaintEvent*)
	{
		QPainter p(this);

		Ya::VisualUtil::paintRosterBackground(this, &p);

		QLinearGradient linearGrad(
			QPointF(rect().left(), rect().top()), QPointF(rect().right(), rect().bottom())
		);
		QColor transparent(Qt::white);
		transparent.setAlpha(0);
		linearGrad.setColorAt(0, Qt::white);
		linearGrad.setColorAt(0.4, Qt::white);
		linearGrad.setColorAt(1, transparent);
		p.fillRect(rect(), linearGrad);
	}
};
#endif

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
	verticalScrollBar()->setSingleStep(1);

	// setItemDelegate(new PsiContactListViewDelegate(this));

#ifdef Q_WS_MAC
	setFrameShape(QFrame::NoFrame);
#endif

#ifdef YAPSI
	setCornerWidget(new ContactListViewCorner());
#endif

	connect(this, SIGNAL(activated(const QModelIndex&)), SLOT(itemActivated(const QModelIndex&)));
	// showStatus_ = PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.show").toBool();
}

static void setExpandedState(QTreeView* view, QAbstractItemModel* model, const QModelIndex& parent)
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

void ContactListView::doItemsLayout()
{
	if (!model())
		return;
	HoverableTreeView::doItemsLayout();
	updateGroupExpandedState();
}

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

	if (contextMenu_)
		delete contextMenu_;
	contextMenu_ = 0;

	// FIXME: need to implement context menu merging
	if (selectedIndexes().count() == 1) {
		ContactListItemProxy* item = itemProxy(selectedIndexes().first());
		if (item) {
			contextMenu_ = createContextMenuFor(item->item());
			addContextMenuActions();
		}
	}
}

ContactListItemMenu* ContactListView::createContextMenuFor(ContactListItem* item) const
{
	if (item)
		return item->contextMenu(dynamic_cast<ContactListModel*>(realModel()));
	return 0;
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
		foreach(QAction* action, contextMenu_->availableActions()) {
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
	if (dynamic_cast<ContactListModel*>(connectToModel)) {
		connect(this, SIGNAL(expanded(const QModelIndex&)), SLOT(itemExpanded(const QModelIndex&)));
		connect(this, SIGNAL(collapsed(const QModelIndex&)), SLOT(itemCollapsed(const QModelIndex&)));
		connect(this, SIGNAL(realExpanded(const QModelIndex&)), connectToModel, SLOT(expanded(const QModelIndex&)));
		connect(this, SIGNAL(realCollapsed(const QModelIndex&)), connectToModel, SLOT(collapsed(const QModelIndex&)));
		connect(connectToModel, SIGNAL(inPlaceRename()), SLOT(rename()));

		// connection to showOfflineChanged() should be established after the proxy model does so,
		// otherwise we won't get consistently re-expanded groups. Qt::QueuedConnection could
		// be advised for in such case.
		connect(connectToModel, SIGNAL(showOfflineChanged()), SLOT(showOfflineChanged()));
	}
}

void ContactListView::resizeEvent(QResizeEvent* e)
{
	HoverableTreeView::resizeEvent(e);
	if (header()->count() > 0)
		header()->resizeSection(0, viewport()->width());
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
				emit activated(currentIndex());
		}
		else {
			event->ignore();
		}
		break;
	case Qt::Key_Space:
		if (state() != EditingState) {
			if (ContactListModel::isGroupType(currentIndex())) {
				toggleExpandedState(currentIndex());
			}
			else {
				QContextMenuEvent e(QContextMenuEvent::Keyboard,
							visualRect(currentIndex()).center());
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
	QModelIndex parent;
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
	QSortFilterProxyModel* proxyModel = dynamic_cast<QSortFilterProxyModel*>(model);
	if (proxyModel)
		return realModel(proxyModel->sourceModel());
	return model;
}

QAbstractItemModel* ContactListView::realModel() const
{
	return ::realModel(model());
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
	QSortFilterProxyModel* proxyModel = dynamic_cast<QSortFilterProxyModel*>(model);
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

QModelIndex ContactListView::proxyIndex(const QModelIndex& index) const
{
	return ::proxyIndex(model(), index);
}

ContactListItemProxy* ContactListView::itemProxy(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;
	return static_cast<ContactListItemProxy*>(realIndex(index).internalPointer());
}

QLineEdit* ContactListView::currentEditor() const
{
	QWidget* w = indexWidget(currentIndex());
	return dynamic_cast<QLineEdit*>(w);
}

void ContactListView::setEditingIndex(const QModelIndex& index, bool editing) const
{
	ContactListItemProxy* itemProxy = this->itemProxy(index);
	if (itemProxy) {
		itemProxy->item()->setEditing(editing);
	}

	ContactListModel* contactListModel = dynamic_cast<ContactListModel*>(realModel());
	if (contactListModel) {
		contactListModel->setUpdatesEnabled(!editing);
	}
}

void ContactListView::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
	setEditingIndex(currentIndex(), false);

	HoverableTreeView::closeEditor(editor, hint);
}

void ContactListView::saveCurrentGroupOrder(const QModelIndex& parent)
{
	ContactListModel* contactListModel = dynamic_cast<ContactListModel*>(realModel());
	Q_ASSERT(contactListModel);
	if (!contactListModel)
		return;

	QModelIndexList groups;
	for (int row = 0; row < this->model()->rowCount(parent); ++row) {
		QModelIndex i = this->model()->index(row, parent.column(), parent);
		if (ContactListModel::indexType(i) == ContactListModel::GroupType) {
			groups << i;

			saveCurrentGroupOrder(i);
		}
	}

	foreach(QModelIndex i, groups) {
		contactListModel->setGroupOrder(i.data(ContactListModel::FullGroupNameRole).toString(),
		                     groups.indexOf(i));
	}
}

// TODO: FIXME: need to adapt the code in order to make it work with nested groups
void ContactListView::commitData(QWidget* editor)
{
	QString newGroupName;
	int groupOrder = 0;

	QLineEdit* lineEdit = currentEditor();
	ContactListModel* contactListModel = dynamic_cast<ContactListModel*>(realModel());
	if (lineEdit && contactListModel && contactListModel->indexType(realIndex(currentIndex())) == ContactListModel::GroupType) {
		// TODO: deal with nested groups too!
		newGroupName = lineEdit->text();
		groupOrder = contactListModel->groupOrder(currentIndex().data(ContactListModel::FullGroupNameRole).toString());
	}

	HoverableTreeView::commitData(editor);

	if (!newGroupName.isEmpty() && contactListModel) {
		setEditingIndex(currentIndex(), false);
		contactListModel->updaterCommit();
		contactListModel->setGroupOrder(newGroupName, groupOrder);

		ContactListGroup* group = contactListModel->groupCache()->findGroup(newGroupName);
		if (group) {
			group->updateOnlineContactsFlag();
		}
		ContactListGroup* parent = group ? group->parent() : 0;
		int i = (group && parent) ? parent->indexOf(group) : -1;
		ContactListItemProxy* proxy = (parent && (i >= 0)) ? parent->item(i) : 0;

		saveCurrentGroupOrder(QModelIndex());

		if (proxy) {
			// we don't want doItemsLayout() to restore a previous selection (now out of date)
			doItemsLayout();

			QModelIndex index = contactListModel->itemProxyToModelIndex(proxy);
			QModelIndex proxyIndex = this->proxyIndex(index);
			if (proxyIndex.isValid()) {
#ifdef YAPSI
				dynamic_cast<SmoothScrollBar*>(verticalScrollBar())->setEnableSmoothScrolling(false);
#endif
				setCurrentIndex(proxyIndex);
				scrollTo(proxyIndex);
#ifdef YAPSI
				dynamic_cast<SmoothScrollBar*>(verticalScrollBar())->setEnableSmoothScrolling(true);
#endif
			}
			else {
				// TODO: proxy model somehow is not always updated
			}
		}
	}
	// setFocus(); // commented out in order to avoid deep recursion
	updateContextMenu();
}

void ContactListView::ensureVisible(const QModelIndex& index)
{
	scrollTo(index, QAbstractItemView::EnsureVisible);
}
