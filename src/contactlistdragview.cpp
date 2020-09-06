/*
 * contactlistdragview.cpp - ContactListView with support for Drag'n'Drop operations
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

#include "contactlistdragview.h"

#include "contactlistdragmodel.h"
#include "contactlistitem.h"
#include "contactlistitemmenu.h"
#include "contactlistmodelselection.h"
#include "contactlistviewdelegate.h"
#include "iconaction.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "debug.h"
#include "textutil.h"
#include "psicontact.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QMessageBox>
#include <QPainterPath>

ContactListDragView::ContactListDragView(QWidget* parent)
	: ContactListView(parent)
	, backedUpSelection_(0)
	, removeAction_(0)
	, dropIndicatorRect_(QRect())
	, dropIndicatorPosition_(QAbstractItemView::OnViewport)
	, keyboardModifiers_(Qt::NoModifier)
	, dirty_(false)
	, pressedIndex_(0)
	, pressedIndexWasSelected_(false)
	, viewportMenu_(0)
	, editing(false)
{
	removeAction_ = new IconAction("", "psi/remove", QString(), ShortcutManager::instance()->shortcuts("contactlist.delete"), this, "act_remove");
	connect(removeAction_, SIGNAL(triggered()), SLOT(removeSelection()));
	addAction(removeAction_);

	connect(this, SIGNAL(entered(const QModelIndex&)), SLOT(updateCursorMouseHover(const QModelIndex&)));
	connect(this, SIGNAL(clicked(const QModelIndex&)), SLOT(itemClicked(const QModelIndex&)));
	connect(this, SIGNAL(viewportEntered()), SLOT(updateCursorMouseHover()));

	setSelectionMode(ExtendedSelection);
	viewport()->installEventFilter(this);

	setDragEnabled(true);
	setAcceptDrops(true);
	setDropIndicatorShown(false); // we're painting it by ourselves
}

ContactListDragView::~ContactListDragView()
{
	delete backedUpSelection_;
	delete pressedIndex_;
}

/**
 * Make sure that all keyboard shortcuts are unique in order to avoid
 * "QAction::eventFilter: Ambiguous shortcut overload: Del" messages.
 */
void ContactListDragView::addContextMenuAction(QAction* action)
{
	QMenu* menu = action->menu();
	if (menu) {
		// if the action contains a menu, add the menu's actions instead
		foreach(QAction* subact, menu->actions()) {
			addContextMenuAction(subact);
		}
	}
	else
	{
		foreach(QAction* act, findChildren<QAction*>()) {
			// TODO: maybe check individual shortcuts too?
			if (act->shortcuts() == action->shortcuts()) {
				return;
			}
		}

		ContactListView::addContextMenuAction(action);
	}
}

void ContactListDragView::setItemDelegate(QAbstractItemDelegate* delegate)
{
	QAbstractItemDelegate *oldDelegate = itemDelegate();
	if (delegate == oldDelegate)
		return;
	if (delegate) {
		connect(delegate, SIGNAL(commitData(QWidget*)), this, SLOT(finishedEditing()));
		connect(delegate, SIGNAL(closeEditor(QWidget*)), this, SLOT(finishedEditing()));
	}
	ContactListView::setItemDelegate(delegate);
	if (oldDelegate)
		delete oldDelegate;
	modelChanged();
//	doItemsLayout();
}

void ContactListDragView::finishedEditing()
{
	editing = false;
}

bool ContactListDragView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
	if (ContactListView::edit(index, trigger, event)) {
		editing = true;
		return true;
	}
	return false;
}

void ContactListDragView::leaveEvent(QEvent* e)
{
	ContactListView::leaveEvent(e);
	updateCursorMouseHover();
}

int ContactListDragView::suggestedItemHeight()
{
	int rowCount = model()->rowCount(QModelIndex());
	if (!rowCount)
		return 0;
	return qMin(viewport()->height() / rowCount - 2, dynamic_cast<ContactListViewDelegate*>(itemDelegate())->avatarSize());
}

void ContactListDragView::mouseDoubleClickEvent(QMouseEvent* e)
{
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model && pressedIndex_) {
		QModelIndexList indexes = model->indexesFor(0, pressedIndex_);
		if (e->button() == Qt::LeftButton
			&& indexes.count() == 1
			&& model->toItem(indexes.first())->isGroup()) {

			return;
		}
	}

	if (!activateItemsOnSingleClick()) {
		ContactListView::mouseDoubleClickEvent(e);
	}
}

void ContactListDragView::itemActivated(const QModelIndex& index)
{
	Q_ASSERT(index.isValid());

	if (!index.isValid())
		return;

	if (realModel()->toItem(realIndex(index))->isGroup()
		&& activateItemsOnSingleClick()) {

		toggleExpandedState(index);
		return;
	}

	ContactListView::itemActivated(index);
}

/**
 * Returns a list of real-model indexes.
 */
QModelIndexList ContactListDragView::indexesFor(PsiContact* contact, QMimeData* contactSelection) const
{
	QModelIndexList indexes;
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model) {
		indexes = model->indexesFor(contact, contactSelection);
	}
	return indexes;
}

void ContactListDragView::toolTipEntered(PsiContact* contact, QMimeData* contactSelection)
{
	Q_UNUSED(contact); // we don't want tooltips on multiple selections
	QModelIndexList indexes = indexesFor(0, contactSelection);
	if (indexes.count() == 1) {
		updateCursor(proxyIndex(indexes.first()), UC_TooltipEntered, false);
	}
}

void ContactListDragView::toolTipHidden(PsiContact* contact, QMimeData* contactSelection)
{
	Q_UNUSED(contact);
	Q_UNUSED(contactSelection);
	if (!drawSelectionBackground())
		updateCursor(QModelIndex(), UC_TooltipHidden, false);
}

void ContactListDragView::updateCursorMouseHover()
{
	updateCursor(QModelIndex(), UC_MouseHover, false);
}

bool ContactListDragView::updateCursor(const QModelIndex& index, UpdateCursorOrigin origin, bool force)
{
	if (isContextMenuVisible()     ||
	    extendedSelectionAllowed() ||
	    state() != NoState)
	{
		if (!force) {
			return false;
		}
	}

	ContactListItem::Type type = qvariant_cast<ContactListItem::Type>(index.data(ContactListModel::TypeRole));

	if (!index.isValid() || type == ContactListItem::Type::ContactType)
		setCursor(Qt::ArrowCursor);
	else
		setCursor(Qt::PointingHandCursor);

	if (origin == UC_MouseClick) {
		if (index.isValid())
			setCurrentIndex(index);
		else
			clearSelection();
	}

	viewport()->update();
	return true;
}

void ContactListDragView::updateCursorMouseHover(const QModelIndex& index)
{
	updateCursor(index, UC_MouseHover, false);
}

int ContactListDragView::indexCombinedHeight(const QModelIndex& parent, QAbstractItemDelegate* delegate) const
{
	if (!delegate || !model())
		return 0;
	int result = delegate->sizeHint(QStyleOptionViewItem(), parent).height();
	for (int row = 0; row < model()->rowCount(parent); ++row) {
		QModelIndex index = model()->index(row, 0, parent);
		if (isExpanded(index))
			result += indexCombinedHeight(index, delegate);
		else
			result += delegate->sizeHint(QStyleOptionViewItem(), index).height();
	}
	return result;
}

void ContactListDragView::setModel(QAbstractItemModel* newModel)
{
	ContactListView::setModel(newModel);

	connect(verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrollbarValueChanged()));
}

bool ContactListDragView::textInputInProgress() const
{
	return state() == QAbstractItemView::EditingState;
}

void ContactListDragView::paintEvent(QPaintEvent* e)
{
	// Q_ASSERT(!dirty_);
	// if (dirty_)
	// 	return;
	ContactListView::paintEvent(e);

	// drawDragIndicator!!
	if (state() == QAbstractItemView::DraggingState &&
	    viewport()->cursor().shape() != Qt::ForbiddenCursor &&
	    !dropIndicatorRect_.isEmpty())
	{
		QPainter p(viewport());
		// QColor dragIndicatorColor = QColor(0x17, 0x0B, 0xFF);
		QColor dragIndicatorColor = QColor(0x00, 0x00, 0x00);
		p.setPen(QPen(dragIndicatorColor, 2));
		p.setRenderHint(QPainter::Antialiasing, true);
		QPainterPath path;
		path.addRoundRect(dropIndicatorRect_.adjusted(2, 1, 0, 0), 5);
		p.drawPath(path);
	}
}

bool ContactListDragView::supportsDropOnIndex(QDropEvent *e, const QModelIndex &index) const
{
	if (!index.isValid()) {
		return false;
	}

	ContactListModelSelection selection(e->mimeData());
	ContactListDragModel *model = qobject_cast<ContactListDragModel*>(realModel());
	if (!selection.haveRosterSelection())
		model = 0;

	if (model && !model->supportsMimeDataOnIndex(e->mimeData(), realIndex(index))) {
		return false;
	}

	if (dropPosition(e, selection, index) == OnViewport) {
		return false;
	}

	return true;
}

static void updateDefaultDropAction(QDropEvent* e)
{
	if (e->keyboardModifiers() == Qt::NoModifier) {
		e->setDropAction(Qt::MoveAction);
	}
}

static void acceptDropAction(QDropEvent* e)
{
	if (e->keyboardModifiers() == Qt::NoModifier) {
		if (e->dropAction() != Qt::MoveAction) {
			//qWarning("acceptDropAction(): e->dropAction() != Qt::MoveAction");
			return;
		}
		Q_ASSERT(e->dropAction() == Qt::MoveAction);
		e->accept();
	}
	else {
		e->acceptProposedAction();
	}
}

void ContactListDragView::dragMoveEvent(QDragMoveEvent* e)
{
	QModelIndex index = indexAt(e->pos());
	dropIndicatorRect_ = QRect();
	dropIndicatorPosition_ = OnViewport;

	// this is crucial at least for auto-scrolling, possibly other things
	ContactListView::dragMoveEvent(e);

	if (supportsDropOnIndex(e, index)) {
		bool ok = true;
		ContactListModelSelection selection(e->mimeData());
		dropIndicatorPosition_ = dropPosition(e, selection, index);
		if (dropIndicatorPosition_ == AboveItem || dropIndicatorPosition_ == BelowItem) {
			dropIndicatorRect_ = groupReorderDropRect(dropIndicatorPosition_, selection, index);
		}
		else if (dropIndicatorPosition_ == OnItem) {
			dropIndicatorRect_ = onItemDropRect(index);
		}
		else {
			ok = false;
		}

		if (ok) {
			updateDefaultDropAction(e);
			acceptDropAction(e);
			return;
		}
	}

	e->ignore();
	viewport()->update();
}

void ContactListDragView::scrollbarValueChanged()
{
	dropIndicatorRect_ = QRect();
	dropIndicatorPosition_ = OnViewport;
	viewport()->update();
	updateCursorMouseHover(underMouse() ? indexAt(mousePosition()) : QModelIndex());
}

void ContactListDragView::dragEnterEvent(QDragEnterEvent* e)
{
	ContactListView::dragEnterEvent(e);
	updateDefaultDropAction(e);
	acceptDropAction(e);
}

void ContactListDragView::dragLeaveEvent(QDragLeaveEvent* e)
{
	ContactListView::dragLeaveEvent(e);
}

void ContactListDragView::dropEvent(QDropEvent* e)
{
	updateDefaultDropAction(e);

	QModelIndex index = indexAt(e->pos());
	dropIndicatorRect_ = QRect();

	if (dropIndicatorPosition_ == OnViewport || !supportsDropOnIndex(e, index)) {
		e->ignore();
		viewport()->update();
		return;
	}

	if (dropIndicatorPosition_ == OnItem) {
		if (model()->dropMimeData(e->mimeData(),
		                          e->dropAction(), -1, -1, index)) {
			acceptDropAction(e);
		}
	}
	else if (dropIndicatorPosition_ == AboveItem || dropIndicatorPosition_ == BelowItem) {
		reorderGroups(e, index);
		acceptDropAction(e);
	}

	stopAutoScroll();
	setState(NoState);
	viewport()->update();
}

QAbstractItemView::DropIndicatorPosition ContactListDragView::dropPosition(QDropEvent* e, const ContactListModelSelection& selection, const QModelIndex& index) const
{
	if ((selection.groups().count() > 0 || selection.accounts().count() > 0) && selection.contacts().count() == 0) {
		// TODO: return OnItem in case the special modifiers are pressed
		// even in case there are multiple groups selected
		// in order to create nested groups
		int totalSelectedGroups = selection.groups().count() + selection.accounts().count();
		if (totalSelectedGroups == 1) {
			QModelIndex group = itemToReorderGroup(selection, index);
			if (group.isValid()) {
				QRect rect = groupVisualRect(group);
				if (e->pos().y() >= rect.center().y()) {
					return BelowItem;
				}
				else {
					return AboveItem;
				}
			}
		}

		return OnViewport;
	}

	if (selection.groups().count() + selection.contacts().count() > 0)
		return OnItem;
	else
		return OnViewport;
}

struct OrderedGroup {
	QString name;
	int order;
};

void ContactListDragView::reorderGroups(QDropEvent *e, const QModelIndex &index)
{
	ContactListModelSelection selection(e->mimeData());
	QModelIndex item = itemToReorderGroup(selection, index);
	if (!item.isValid()) {
		return;
	}
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	Q_ASSERT(model);
	Q_ASSERT(selection.groups().count() > 0 || selection.accounts().count() > 0);
	Q_ASSERT(selection.contacts().count() == 0);
	QModelIndexList selectedGroups = model->indexesFor(&selection);
	if (!selectedGroups.count())
		return;

	// we need to work in terms of proxy indexes because we need actually-sorted items
	QModelIndex selectedGroup = proxyIndex(selectedGroups.first());
	QModelIndex groupParent = selectedGroup.parent();

	QModelIndexList groups;
	for (int row = 0; row < this->model()->rowCount(groupParent); ++row) {
		QModelIndex index = this->model()->index(row, selectedGroup.column(), groupParent);
		if (model->toItem(index)->isGroup()) {
			groups << index;
		}
	}

	// re-order
	groups.removeAll(selectedGroup);
	if (dropIndicatorPosition_ == AboveItem) {
		groups.insert(groups.indexOf(item), selectedGroup);
	}
	else {
		Q_ASSERT(dropIndicatorPosition_ == BelowItem);
		groups.insert(groups.indexOf(item) + 1, selectedGroup);
	}

//	foreach(QModelIndex i, groups) {
//		model->setGroupOrder(i.data(ContactListModel::FullGroupNameRole).toString(),
//		                     groups.indexOf(i));
//	}
}

QModelIndex ContactListDragView::itemToReorderGroup(const ContactListModelSelection& selection, const QModelIndex& index) const
{
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	Q_ASSERT(model);
//	ContactListModel::Type groupType =
//	    selection.groups().count() == 1 ?
//	    ContactListModel::GroupType :
//	    ContactListModel::AccountType;
	Q_ASSERT(selection.groups().count() == 1 || selection.accounts().count() == 1);
	QModelIndexList selectedGroups = model->indexesFor(&selection);
	if (!selectedGroups.count())
		return QModelIndex();
	QModelIndex selectedGroup = selectedGroups.first();
	// we need to work in terms of proxy indexes because we need actually-sorted items
	selectedGroup = proxyIndex(selectedGroup);
	const QModelIndex groupParent = selectedGroup.parent();

	QModelIndex indexParent = index;
	while (indexParent.isValid() && indexParent.parent() != groupParent) {
		indexParent = indexParent.parent();
	}
	if (indexParent.parent() != groupParent || indexParent == selectedGroup)
		return QModelIndex();
//	if (static_cast<ContactListGroup::SpecialType>(indexParent.data(ContactListModel::SpecialGroupTypeRole).toInt()) != ContactListGroup::SpecialType_None)
//		return QModelIndex();

	// this code is necessary if we allow contacts on the same level as group items
//	bool breakAtFirstGroup = (ContactListModel::indexType(index) != groupType) && index.parent() == groupParent;
	QModelIndex result;
	for (int row = 0; row < this->model()->rowCount(groupParent); ++row) {
		result = this->model()->index(row, indexParent.column(), groupParent);
//		if (breakAtFirstGroup) {
//			if (ContactListModel::indexType(result) == groupType)
//				break;
//		}
//		else if (result.row() == indexParent.row()) {
//			break;
//		}
	}
	return result;
}

QRect ContactListDragView::groupReorderDropRect(DropIndicatorPosition dropIndicatorPosition, const ContactListModelSelection& selection, const QModelIndex& index) const
{
	QModelIndex group = itemToReorderGroup(selection, index);
	QRect r(groupVisualRect(group));
	if (dropIndicatorPosition == AboveItem) {
		return QRect(r.left(), r.top(), r.width(), 1);
	}
	else {
		Q_ASSERT(dropIndicatorPosition == BelowItem);
		QRect result(r.left(), r.bottom(), r.width(), 1);
		if (result.bottom() < viewport()->height() - 2)
			result.translate(0, 2);
		return result;
	}
}

QRect ContactListDragView::onItemDropRect(const QModelIndex& index) const
{
	if (!index.isValid()) {
		return viewport()->rect().adjusted(0, 0, -1, -1);
	}

	ContactListItem *item = realModel()->toItem(realIndex(index));

	if (item->isContact()) {
		return onItemDropRect(index.parent());
	}
	else {
		return groupVisualRect(index);
	}
}

QRect ContactListDragView::groupVisualRect(const QModelIndex& index) const
{
	Q_ASSERT(realModel()->toItem(realIndex(index))->isGroup());

	QRect result;
	combineVisualRects(index, &result);
	return result.adjusted(0, 0, -1, -1);
}

void ContactListDragView::combineVisualRects(const QModelIndex& parent, QRect* result) const
{
	Q_ASSERT(result);
	*result = result->united(visualRect(parent));
	for (int row = 0; row < model()->rowCount(parent); ++row) {
		QModelIndex index = model()->index(row, 0, parent);
		combineVisualRects(index, result);
	}
}

void ContactListDragView::startDrag(Qt::DropActions supportedActions)
{
	ContactListView::startDrag(supportedActions);
}

// Return ContactListModelSelection
QMimeData* ContactListDragView::selection() const
{
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model && model->contactList() && !selectedIndexes().isEmpty()) {
		return model->mimeData(realIndexes(selectedIndexes()));
	}
	return 0;
}

void ContactListDragView::restoreSelection(QMimeData* _mimeData)
{
	// setCurrentIndex() actually fires up the timers which in turn could trigger
	// some delayed events that would result in mimeData deletion (if the mimeData
	// passed was actually backedUpSelection_). So for additional insurance if
	// clearSelection() suddenly decides to trigger timers too we put mimeData
	// into a QPointer
	QPointer<QMimeData> mimeData(_mimeData);
	setUpdatesEnabled(false);

	// setCurrentIndex(QModelIndex());
	clearSelection();

	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model && !mimeData.isNull()) {
		QModelIndexList indexes = proxyIndexes(model->indexesFor(mimeData));
		if (!indexes.isEmpty()) {
			setCurrentIndex(indexes.first());
			QItemSelection selection;
			foreach(QModelIndex index, indexes)
				selection << QItemSelectionRange(index);
			selectionModel()->select(selection, QItemSelectionModel::Select);
		}
	}

	setUpdatesEnabled(true);
}

ContactListItemMenu* ContactListDragView::createContextMenuFor(ContactListItem* item) const
{
	ContactListItemMenu* menu = ContactListView::createContextMenuFor(item);
	if (menu) {
		if (menu->metaObject()->indexOfSignal("removeSelection()") != -1)
			connect(menu, SIGNAL(removeSelection()), SLOT(removeSelection()));
	}
	return menu;
}

void ContactListDragView::removeSelection()
{
	SLOW_TIMER(100);
	QList<PsiContact*> contacts;
	QStringList contactNames;

	QMimeData *mimeData = selection();
	QModelIndexList indexes = qobject_cast<ContactListDragModel*>(realModel())->indexesFor(mimeData);
	delete mimeData;

	for (const QModelIndex &index: indexes) {
		ContactListItem *item = realModel()->toItem(index);

		if (item->isContact())
			contacts << item->contact();
		else if (item->isGroup())
			contacts += item->contacts();
	}

	if (contacts.isEmpty())
		return;

	// Ask for deleting only some contacts. Exclude private contacts and not in list contacts
	for (PsiContact *contact: contacts) {
		QString name = contact->name();
		if (name != contact->jid().full()) {
			name = tr("%1 (%2)").arg(name, TextUtil::escape(contact->jid().full()));
		}

		if (!contact->isPrivate() && contact->inList()) {
			contactNames << QString("<b>%1</b>").arg(TextUtil::escape(name));
		}
	}

	bool doRemove = true;

	if (!contactNames.isEmpty()) {
		QString message = tr("This will permanently remove<br>"
							 "%1"
							 "<br>from your contact list."
							 ).arg(contactNames.join(", "));

		QMessageBox box(QMessageBox::Icon::Warning, tr("Deleting contacts"), message, QMessageBox::StandardButton::Cancel | QMessageBox::StandardButton::Yes, this);
		box.button(QMessageBox::StandardButton::Yes)->setText(tr("Delete"));
		doRemove = static_cast<QMessageBox::StandardButton>(box.exec()) == QMessageBox::StandardButton::Yes;
	}

	if (doRemove) {
		for (PsiContact *contact: contacts) {
			contact->remove();
		}
	}
}

bool ContactListDragView::extendedSelectionAllowed() const
{
	return selectedIndexes().count() > 1 || keyboardModifiers_ != 0;
}

bool ContactListDragView::activateItemsOnSingleClick() const
{
// 	return style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, this);
	return PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool();
}

void ContactListDragView::updateKeyboardModifiers(const QEvent* e)
{
	if (e->type() == QEvent::KeyPress ||
	    e->type() == QEvent::KeyRelease ||
	    e->type() == QEvent::MouseButtonDblClick ||
	    e->type() == QEvent::MouseButtonPress ||
	    e->type() == QEvent::MouseButtonRelease ||
	    e->type() == QEvent::MouseMove)
	{
		keyboardModifiers_ = static_cast<const QInputEvent*>(e)->modifiers();
	}
}

Qt::KeyboardModifiers ContactListDragView::keyboardModifiers() const
{
	return keyboardModifiers_;
}

bool ContactListDragView::eventFilter(QObject* obj, QEvent* e)
{
	if (obj == this || obj == viewport()) {
		updateKeyboardModifiers(e);
	}

	return ContactListView::eventFilter(obj, e);
}

void ContactListDragView::mousePressEvent(QMouseEvent* e)
{
	pressPosition_ = e->pos();
	delete pressedIndex_;
	pressedIndex_ = 0;
	pressedIndexWasSelected_ = false;

	QModelIndex index = indexAt(e->pos());
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model && index.isValid()) {
		QModelIndexList indexes;
		indexes << realIndex(index);
		pressedIndex_ = model->mimeData(indexes);
	}
	pressedIndexWasSelected_ = e->button() == Qt::LeftButton &&
	                           selectionModel() &&
	                           selectionModel()->isSelected(index);
	if (!index.isValid()) {
		// clear selection
		updateCursor(index, UC_MouseClick, true);
	}
	if (e->button() == Qt::MiddleButton && index.isValid()) {
		itemActivated(index);
	}
	ContactListView::mousePressEvent(e);

	static const int iconSize = 16;
	bool iconClick = e->pos().x() < iconSize + 2;

	if (( activateItemsOnSingleClick() || iconClick ) && e->button() == Qt::LeftButton) {
		ContactListView::mouseDoubleClickEvent(e);
	}
}

void ContactListDragView::mouseMoveEvent(QMouseEvent* e)
{
	// don't allow any selections after single-clicking on the group button
	if (!pressedIndex_ && !pressedIndexWasSelected_ && e->buttons() & Qt::LeftButton) {
		e->accept();
		return;
	}

	ContactListView::mouseMoveEvent(e);
}

void ContactListDragView::itemClicked(const QModelIndex& index)
{
	if (activateItemsOnSingleClick())
		itemActivated(index);

	// if clicked item was selected prior to mouse press event, activate it on release
	// if (pressedIndexWasSelected_) {
	// 	activate(index);
	// }
	Q_UNUSED(index);
}

void ContactListDragView::contextMenuEvent(QContextMenuEvent* e)
{
	QModelIndex index = indexAt(e->pos());
	if (!selectedIndexes().contains(index)) {
		updateCursor(index, UC_MouseClick, true);
	}

	ContactListView::contextMenuEvent(e);

	if (!e->isAccepted() && viewportMenu_) {
		viewportMenu_->exec(e->globalPos());
	}

	repairMouseTracking();
}

bool ContactListDragView::drawSelectionBackground() const
{
	return focusPolicy() == Qt::NoFocus     ||
	       testAttribute(Qt::WA_UnderMouse) ||
	       extendedSelectionAllowed();
}

void ContactListDragView::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
	ContactListView::closeEditor(editor, hint);
}

void ContactListDragView::backupCurrentSelection()
{
	if (backedUpSelection_) {
		delete backedUpSelection_;
		backedUpSelection_ = 0;
	}

	backedUpSelection_ = selection();
}

void ContactListDragView::restoreBackedUpSelection()
{
	restoreSelection(backedUpSelection_);
}

void ContactListDragView::modelChanged()
{
	if (!dirty_) {
//		setUpdatesEnabled(false);
		if (currentEditor() && editing) {
			backedUpEditorValue_ = currentEditor()->text();
			closeEditor(currentEditor(), QAbstractItemDelegate::NoHint);
			setEditingIndex(currentIndex(), true);
		}
		else {
			backedUpEditorValue_ = QString();
		}
		dirty_ = true;
		scheduleDelayedItemsLayout();
	}

	// TODO: save the object we're currently editing and the QVariant of the value the user inputted

	// make sure selectionModel() doesn't cache any currently selected indexes
	// otherwise it'll overwrite our correctly restored selection with its own
//	if (selectionModel()) {
//		selectionModel()->reset();
//	}

	updateContextMenu();
}

void ContactListDragView::updateGeometries()
{
	ContactListView::updateGeometries();
}

/**
 * Prevent emitting activated events when clicking with right mouse button.
 */
void ContactListDragView::mouseReleaseEvent(QMouseEvent* event)
{
	bool filter = false;
	ContactListDragModel* model = qobject_cast<ContactListDragModel*>(realModel());
	if (model && pressedIndex_) {
		QModelIndexList indexes = model->indexesFor(0, pressedIndex_);
		QModelIndex index = indexes.count() == 1 ? proxyIndex(indexes.first()) : QModelIndex();
		if (event->button() == Qt::LeftButton &&
		    index.isValid() &&
		    keyboardModifiers() == 0 &&
		    model->toItem(realIndex(index))->isGroup() &&
		    activateItemsOnSingleClick()) {

			if ((pressPosition_ - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
				QStyleOptionViewItem option;
				toggleExpandedState(index);
				event->accept();
				filter = true;
			}
		}
	}

	if ((!filter && event->button() & Qt::LeftButton) || !activateItemsOnSingleClick()) {
		ContactListView::mouseReleaseEvent(event);
	}

	pressPosition_ = QPoint();
	delete pressedIndex_;
	pressedIndex_ = 0;
}

void ContactListDragView::setViewportMenu(QMenu* menu)
{
	viewportMenu_ = menu;
}
