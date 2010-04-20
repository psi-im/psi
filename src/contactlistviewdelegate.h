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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONTACTLISTVIEWDELEGATE_H
#define CONTACTLISTVIEWDELEGATE_H

#include <QItemDelegate>

#include "contactlistmodel.h"

#include "hoverabletreeview.h"
#include "xmpp_status.h"

class ContactListView;
class ContactListItemProxy;
class PsiContact;
class ContactListGroup;
class PsiAccount;
class QStyleOptionViewItemV2;

class ContactListViewDelegate : public QItemDelegate
{
public:
	ContactListViewDelegate(ContactListView* parent);
	virtual ~ContactListViewDelegate();

	virtual int avatarSize() const;
	virtual ContactListView* contactList() const;

	// reimplemented
	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual void getEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index, QRect* widgetRect, QRect* lineEditRect) const;
	virtual void setEditorData(QWidget* editor, const QModelIndex& index) const;
	virtual void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;

	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	void doSetOptions(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	virtual int horizontalMargin() const;
	virtual int verticalMargin() const;
	virtual void setHorizontalMargin(int margin);
	virtual void setVerticalMargin(int margin);

protected:
	virtual void drawContact(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual void drawGroup(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual void drawAccount(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

	virtual void defaultDraw(QPainter* painter, const QStyleOptionViewItem& option) const;

	virtual void drawText(QPainter* painter, const QStyleOptionViewItem& o, const QRect& rect, const QString& text, const QModelIndex& index) const;
	virtual void drawBackground(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

	virtual QRect nameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const = 0;
	virtual QRect groupNameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const = 0;
	virtual QRect editorRect(const QRect& nameRect) const = 0;
	virtual void setEditorCursorPosition(QWidget* editor, int cursorPosition) const;
	virtual QColor backgroundColor(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QIcon::Mode iconMode() const;
	QIcon::State iconState() const;
	const HoverableStyleOptionViewItem& opt() const;

	virtual QString nameText(const QStyleOptionViewItem& o, const QModelIndex& index) const;
	virtual QString statusText(const QModelIndex& index) const;
	virtual XMPP::Status::Type statusType(const QModelIndex& index) const;

	// reimplemented
	bool eventFilter(QObject* object, QEvent* event);

	// these three are functional only inside paint() call
	bool hovered() const;
	QPoint hoveredPosition() const;
	void setHovered(bool hovered) const;

private:
	class Private;
	Private* d;
	int horizontalMargin_;
	int verticalMargin_;
};

#endif
