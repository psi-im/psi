/*
 * contactlistviewdelegate.cpp - base class for painting contact list items
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

#include "contactlistviewdelegate.h"

#include <limits.h> // for INT_MAX

#include <QKeyEvent>
#include <QPainter>
#include <QLineEdit>

#include "contactlistmodel.h"
#include "contactlistview.h"

#ifdef YAPSI
#include "yavisualutil.h"
#endif

class ContactListViewDelegate::Private
{
public:
	ContactListView* contactList;
	HoverableStyleOptionViewItem opt;
	QIcon::Mode  iconMode;
	QIcon::State iconState;
};

ContactListViewDelegate::ContactListViewDelegate(ContactListView* parent)
	: QItemDelegate(parent)
	, horizontalMargin_(5)
	, verticalMargin_(3)
{
	d = new Private;
	d->contactList = parent;
}

ContactListViewDelegate::~ContactListViewDelegate()
{
	delete d;
}

QIcon::Mode ContactListViewDelegate::iconMode() const
{
	return d->iconMode;
}

QIcon::State ContactListViewDelegate::iconState() const
{
	return d->iconState;
}

const HoverableStyleOptionViewItem& ContactListViewDelegate::opt() const
{
	return d->opt;
}

ContactListView* ContactListViewDelegate::contactList() const
{
	return d->contactList;
}

inline static QIcon::Mode iconMode(QStyle::State state)
{
	if (!(state & QStyle::State_Enabled)) return QIcon::Disabled;
	if   (state & QStyle::State_Selected) return QIcon::Selected;
	return QIcon::Normal;
}

inline static QIcon::State iconState(QStyle::State state)
{
	return state & QStyle::State_Open ? QIcon::On : QIcon::Off;
}

void ContactListViewDelegate::doSetOptions(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	d->opt = setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	d->opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);

	const HoverableStyleOptionViewItem *hoverable = qstyleoption_cast<const HoverableStyleOptionViewItem *>(&option);
	d->opt.hovered = hoverable ? hoverable->hovered : false;
	d->opt.hoveredPosition = hoverable ? hoverable->hoveredPosition : QPoint();

	// see hoverabletreeview.cpp
	if ((d->opt.displayAlignment & Qt::AlignLeft) &&
		(d->opt.displayAlignment & Qt::AlignRight) &&
		(d->opt.displayAlignment & Qt::AlignHCenter) &&
		(d->opt.displayAlignment & Qt::AlignJustify))
	{
		d->opt.hovered = true;
		d->opt.hoveredPosition = QPoint(d->opt.decorationSize.width(), d->opt.decorationSize.height());
	}
}

void ContactListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	doSetOptions(option, index);

	// prepare
	painter->save();
	// if (hasClipping())
	// 	painter->setClipRect(d->opt.rect);

	QVariant value;

	d->iconMode  = ::iconMode(opt().state);
	d->iconState = ::iconState(opt().state);

	switch (ContactListModel::indexType(index)) {
	case ContactListModel::ContactType:
		drawContact(painter, opt(), index);
		break;
	case ContactListModel::GroupType:
		drawGroup(painter, opt(), index);
		break;
	case ContactListModel::AccountType:
		drawAccount(painter, opt(), index);
		break;
	case ContactListModel::InvalidType:
		painter->fillRect(option.rect, Qt::red);
		break;
	default:
		QItemDelegate::paint(painter, option, index);
	}
	painter->restore();

}

void ContactListViewDelegate::defaultDraw(QPainter* painter, const QStyleOptionViewItem& option) const
{
	Q_UNUSED(painter);
	Q_UNUSED(option);
}

int ContactListViewDelegate::avatarSize() const
{
	return 32;
}

void ContactListViewDelegate::drawContact(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(painter);
	Q_UNUSED(option);
	Q_UNUSED(index);
}

void ContactListViewDelegate::drawGroup(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(painter);
	Q_UNUSED(option);
	Q_UNUSED(index);
}

void ContactListViewDelegate::drawAccount(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(painter);
	Q_UNUSED(option);
	Q_UNUSED(index);
}

void ContactListViewDelegate::drawText(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text, const QModelIndex& index) const
{
	if (text.isEmpty())
		return;

	QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
							  ? QPalette::Normal : QPalette::Disabled;
	if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
		cg = QPalette::Inactive;
	if (option.state & QStyle::State_Selected) {
		painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
	}
	else {
		painter->setPen(option.palette.color(cg, QPalette::Text));
	}

	QString txt = text;

	const bool isElided = rect.width() < option.fontMetrics.width(text);
#if 0
	if (isElided) {
#ifndef YAPSI
		txt = option.fontMetrics.elidedText(text, option.textElideMode, rect.width());
#endif
		painter->save();
		QRect txtRect(rect);
		txtRect.setHeight(option.fontMetrics.height());
		painter->setClipRect(txtRect);
	}
#endif
	// painter->save();
	// painter->setPen(Qt::gray);
	// painter->drawLine(rect.left(), rect.top() + option.fontMetrics.ascent(), rect.right(), rect.top() + option.fontMetrics.ascent());
	// painter->restore();

	painter->setFont(option.font);
	QTextOption to;
	to.setWrapMode(QTextOption::NoWrap);
	if (option.direction == Qt::RightToLeft)
		to.setAlignment(Qt::AlignRight);
	painter->drawText(rect, txt, to);

	if (isElided) {
#if 0
		painter->restore();
#endif
// FIXME
#ifdef YAPSI
		Ya::VisualUtil::drawTextFadeOut(painter, rect, backgroundColor(option, index), 15);
#else
		Q_UNUSED(index);
#endif
	}
}

QSize ContactListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(option);
	if (index.isValid()) {
		return QSize(16, avatarSize());
	}

	return QSize(0, 0);
}

// copied from void QItemDelegate::drawBackground(), Qt 4.3.4
QColor ContactListViewDelegate::backgroundColor(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
							  ? QPalette::Normal : QPalette::Disabled;
	if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
		cg = QPalette::Inactive;

	if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
		return option.palette.brush(cg, QPalette::Highlight).color();
	}
	else {
		QVariant value = index.data(Qt::BackgroundRole);
#ifdef HAVE_QT5
		if (value.canConvert<QBrush>()) {
#else
		if (qVariantCanConvert<QBrush>(value)) {
#endif
			return qvariant_cast<QBrush>(value).color();
		}
		else {
			return option.palette.brush(cg, QPalette::Base).color();
		}
	}

	return Qt::white;
}

void ContactListViewDelegate::drawBackground(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt = option;

	// otherwise we're not painting the left item margin sometimes
	// (for example when changing current selection by keyboard)
	opt.rect.setLeft(0);

	{
		if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
			painter->fillRect(opt.rect, backgroundColor(option, index));
		}
		else {
			QPointF oldBO = painter->brushOrigin();
			painter->setBrushOrigin(opt.rect.topLeft());
			painter->fillRect(opt.rect, backgroundColor(option, index));
			painter->setBrushOrigin(oldBO);
		}
	}
}

bool ContactListViewDelegate::hovered() const
{
	return opt().hovered;
}

void ContactListViewDelegate::setHovered(bool hovered) const
{
	d->opt.hovered = hovered;
}

QPoint ContactListViewDelegate::hoveredPosition() const
{
	return opt().hoveredPosition;
}

void ContactListViewDelegate::getEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index, QRect* widgetRect, QRect* lineEditRect) const
{
	Q_UNUSED(editor);
	Q_ASSERT(widgetRect);
	Q_ASSERT(lineEditRect);
	switch (ContactListModel::indexType(index)) {
	case ContactListModel::ContactType:
		*widgetRect = this->nameRect(option, index).adjusted(-1, 0, 0, 1);
		*lineEditRect = *widgetRect;
		*widgetRect = this->editorRect(*widgetRect);
		break;
	case ContactListModel::GroupType:
		*widgetRect = this->groupNameRect(option, index).adjusted(-1, 0, 0, 0);
		widgetRect->setRight(option.rect.right() - horizontalMargin() - 1);
		*lineEditRect = *widgetRect;
		widgetRect->adjust(0, -3, 0, 2);
		break;
	// case ContactListModel::AccountType:
	// 	break;
	default:
		;
	}
}

void ContactListViewDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QRect widgetRect;
	QRect lineEditRect;
	getEditorGeometry(editor, option, index, &widgetRect, &lineEditRect);

	if (!widgetRect.isEmpty()) {
		editor->setGeometry(widgetRect);
	}
}

// we're preventing modifications of QLineEdit while it's still being displayed,
// and the contact we're editing emits dataChanged()
void ContactListViewDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	// same code is in YaContactListViewDelegate::setEditorData
	QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (lineEdit->text().isEmpty()) {
			lineEdit->setText(index.data(Qt::EditRole).toString());
			lineEdit->selectAll();
		}
		return;
	}

	ContactListViewDelegate::setEditorData(editor, index);
}

void ContactListViewDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	// same code is in YaContactListViewDelegate::setModelData
	QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (index.data(Qt::EditRole).toString() != lineEdit->text()) {
			model->setData(index, lineEdit->text(), Qt::EditRole);
		}
	}
	else {
		QItemDelegate::setModelData(editor, model, index);
	}
}

void ContactListViewDelegate::setEditorCursorPosition(QWidget* editor, int cursorPosition) const
{
	QLineEdit* lineEdit = dynamic_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (cursorPosition == -1)
			cursorPosition = lineEdit->text().length();
		lineEdit->setCursorPosition(cursorPosition);
	}
}

// adapted from QItemDelegate::eventFilter()
bool ContactListViewDelegate::eventFilter(QObject* object, QEvent* event)
{
	QWidget* editor = ::qobject_cast<QWidget*>(object);
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Up) {
			setEditorCursorPosition(editor, 0);
			return true;
		}
		else if (keyEvent->key() == Qt::Key_Down) {
			setEditorCursorPosition(editor, -1);
			return true;
		}
		else if (keyEvent->key() == Qt::Key_PageUp ||
				 keyEvent->key() == Qt::Key_PageDown)
		{
			return true;
		}
		else if (keyEvent->key() == Qt::Key_Tab ||
				 keyEvent->key() == Qt::Key_Backtab)
		{
			return true;
		}
	}

	return QItemDelegate::eventFilter(object, event);
}

int ContactListViewDelegate::horizontalMargin() const
{
	return horizontalMargin_;
}

int ContactListViewDelegate::verticalMargin() const
{
	return verticalMargin_;
}

void ContactListViewDelegate::setHorizontalMargin(int margin)
{
	horizontalMargin_ = margin;
}

void ContactListViewDelegate::setVerticalMargin(int margin)
{
	verticalMargin_ = margin;
}

QString ContactListViewDelegate::nameText(const QStyleOptionViewItem& o, const QModelIndex& index) const
{
	Q_UNUSED(o);
	return index.data(Qt::DisplayRole).toString();
}

QString ContactListViewDelegate::statusText(const QModelIndex& index) const
{
	return index.data(ContactListModel::StatusTextRole).toString();
}

XMPP::Status::Type ContactListViewDelegate::statusType(const QModelIndex& index) const
{
	return static_cast<XMPP::Status::Type>(index.data(ContactListModel::StatusTypeRole).toInt());
}
