/*
 * contactlistdragview.h - ContactListView with support for Drag'n'Drop operations
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

#ifndef CONTACTLISTDRAGVIEW_H
#define CONTACTLISTDRAGVIEW_H

#include "contactlistview.h"

class QTimer;
class QMimeData;
class ContactListDragModel;
class ContactListModelSelection;
class PsiContact;

class ContactListDragView : public ContactListView
{
	Q_OBJECT

public:
	ContactListDragView(QWidget* parent);
	~ContactListDragView();

	// reimplemented
	void setModel(QAbstractItemModel* model);

	bool textInputInProgress() const;

	enum AvatarMode {
		AvatarMode_Disable = 0,
		AvatarMode_Auto = 1,
		AvatarMode_Big = 2,
		AvatarMode_Small = 3
	};

	AvatarMode avatarMode() const;
	void setAvatarMode(AvatarMode avatarMode);

	void setViewportMenu(QMenu* menu);

	QMimeData* selection() const;
	void restoreSelection(QMimeData* mimeData);

	bool activateItemsOnSingleClick() const;
	bool extendedSelectionAllowed() const;

	virtual bool drawSelectionBackground() const;
	virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event);

public:
	virtual int suggestedItemHeight();

public slots:
	void toolTipEntered(PsiContact* contact, QMimeData* contactSelection);
	void toolTipHidden(PsiContact* contact, QMimeData* contactSelection);

protected:
	// reimplemented
	void mouseDoubleClickEvent(QMouseEvent*);
	void leaveEvent(QEvent*);
	void paintEvent(QPaintEvent*);
	void dragMoveEvent(QDragMoveEvent*);
	void dropEvent(QDropEvent*);
	void dragEnterEvent(QDragEnterEvent*);
	void dragLeaveEvent(QDragLeaveEvent*);
	void setItemDelegate(QAbstractItemDelegate* delegate);
	void startDrag(Qt::DropActions supportedActions);
	bool eventFilter(QObject* obj, QEvent* e);
	void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint);

protected slots:
	virtual void scrollbarValueChanged();
	void modelChanged();

protected:
	// reimplemented
	virtual void itemActivated(const QModelIndex& index);
	virtual void mousePressEvent(QMouseEvent*);
	virtual void mouseMoveEvent(QMouseEvent*);
	virtual void contextMenuEvent(QContextMenuEvent*);
	virtual ContactListItemMenu* createContextMenuFor(ContactListItem* item) const;
	virtual void addContextMenuAction(QAction* action);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void updateGeometries();

	enum UpdateCursorOrigin {
		UC_MouseClick,
		UC_MouseHover,
		UC_TooltipEntered,
		UC_TooltipHidden
	};

	virtual bool updateCursor(const QModelIndex& index, UpdateCursorOrigin origin, bool force);
	void updateKeyboardModifiers(const QEvent* e);
	Qt::KeyboardModifiers keyboardModifiers() const;
	int indexCombinedHeight(const QModelIndex& index, QAbstractItemDelegate* delegate) const;

private slots:
	void itemClicked(const QModelIndex& index);
	void updateCursorMouseHover(const QModelIndex&);
	void updateCursorMouseHover();
	void removeSelection();
	void finishedEditing();

private:
	QMimeData* backedUpSelection_;
	QString backedUpEditorValue_;
	QAction* removeAction_;
	QRect dropIndicatorRect_;
	DropIndicatorPosition dropIndicatorPosition_;
	Qt::KeyboardModifiers keyboardModifiers_;
	bool dirty_;
	QMimeData* pressedIndex_;
	QPoint pressPosition_;
	bool pressedIndexWasSelected_;
	QMenu* viewportMenu_;
	bool editing;

	void backupCurrentSelection();
	void restoreBackedUpSelection();

	QModelIndexList indexesFor(PsiContact* contact, QMimeData* contactSelection) const;
	QRect onItemDropRect(const QModelIndex& index) const;
	QRect groupReorderDropRect(DropIndicatorPosition dropIndicatorPosition, const ContactListModelSelection& selection, const QModelIndex& index) const;
	QModelIndex itemToReorderGroup(const ContactListModelSelection& selection, const QModelIndex& index) const;
	DropIndicatorPosition dropPosition(QDropEvent* e, const ContactListModelSelection& selection, const QModelIndex& index) const;
	QRect groupVisualRect(const QModelIndex& index) const;
	void combineVisualRects(const QModelIndex& index, QRect* result) const;
	bool supportsDropOnIndex(QDropEvent *e, const QModelIndex &index) const;
	void reorderGroups(QDropEvent *e, const QModelIndex &index);
};

#endif
