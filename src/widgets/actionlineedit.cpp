/*
 * actionlineedit.h - QLineEdit widget with buttons on right side
 * Copyright (C) 2009 Sergey Il'inykh
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <QIcon>
#include <QPainter>
#include <QHBoxLayout>
#include <QAction>
#include <QActionEvent>
#include <QIcon>
#include "actionlineedit.h"

ActionLineEditButton::ActionLineEditButton( QWidget *parent )
		: QAbstractButton(parent), action_(0), popup_(0)
{
	setCursor(Qt::PointingHandCursor);
}

/**
 * Same as for QToolButton
 */
void ActionLineEditButton::setDefaultAction(QAction *act)
{
	action_ = act;
	//TODO what if we set another action. disconnect events...
	connect(this, SIGNAL(clicked()), act, SLOT(trigger()));
	connect(act, SIGNAL(changed()), this, SLOT(updateFromAction()));

	updateFromAction();
}

QAction * ActionLineEditButton::defaultAction()
{
	return action_;
}

void ActionLineEditButton::setPopup(QWidget *widget)
{
	if (!popup_) {
		disconnect(this, SLOT(showPopup())); //TODO test it.
	}
	if (widget) {
		popup_ = widget;
		popup_->setWindowFlags(Qt::Popup);
		connect(this, SIGNAL(clicked()), this, SLOT(showPopup()));
		popup_->adjustSize();
	}
}

void ActionLineEditButton::showPopup()
{
	if (popup_) {
		if (popup_->isHidden()) {
			const QPoint pos = mapToGlobal(QPoint(width()-popup_->width(), height()));
			popup_->move(pos);
			popup_->show();
		}
		else {
			popup_->hide();
		}
	}
}

/**
 * Invlidates button is action changed
 */
void ActionLineEditButton::updateFromAction()
{
	setText(action_->text());
	setIcon(action_->icon());
	setToolTip(action_->toolTip());
	setCheckable(action_->isCheckable());
	setChecked(action_->isChecked());
	setEnabled(action_->isEnabled());
	setVisible(action_->isVisible());
	adjustSize();
}

void ActionLineEditButton::paintEvent ( QPaintEvent * event )
{
	Q_UNUSED(event)

	QPainter painter(this);
	ActionLineEdit *p = (ActionLineEdit *)parent();
	Qt::ToolButtonStyle tbs = p->toolButtonStyle();
	int lpos = 0;
	//int h = height();
	int h = p->minimumSizeHint().height()-2; //2px padding from max.
	if (!icon().isNull() && tbs != Qt::ToolButtonTextOnly) {
		const QPixmap pix = icon().pixmap(
			QSize(h, h),
			isEnabled()?QIcon::Normal:QIcon::Disabled,
			isChecked()?QIcon::On:QIcon::Off
		);
		painter.drawPixmap(0, (height() - pix.height())/2, pix);
		lpos += pix.width();
	}
	if (!(text().isEmpty()) && tbs != Qt::ToolButtonIconOnly) {
		if (lpos) {
			lpos+=2; // notice these 2px also in the sizeHint!
		}
		painter.drawText(QRect(lpos, 0, width()-lpos, height()), Qt::AlignVCenter, text());
	}
}

QSize ActionLineEditButton::sizeHint () const
{
	ActionLineEdit *p = (ActionLineEdit *)parent();
	Qt::ToolButtonStyle tbs = p->toolButtonStyle();
	int w = 0, h = p->height();
	int ih = ((QLineEdit *)parent())->minimumSizeHint().height()-2; //2px padding from max. the same as in paintEvent
	QSize is(0, 0), ts(0, 0);
	if (!icon().isNull() && tbs != Qt::ToolButtonTextOnly) {
		is = icon().actualSize(
			QSize(ih, ih),
			isEnabled()?QIcon::Normal:QIcon::Disabled,
			isChecked()?QIcon::On:QIcon::Off
		);
	}
	if (tbs != Qt::ToolButtonIconOnly) {
		ts = fontMetrics().size(Qt::TextSingleLine, text());
	}
	// 4px padding right, 2px -space between icon and text
	QSize ret = QSize(((w = ts.width())>0?(w+(is.width()?2:0)):0) + is.width() + 4, h);
	return ret;
}

ActionLineEdit::ActionLineEdit(QWidget *parent)
		: QLineEdit(parent), toolButtonStyle_(Qt::ToolButtonIconOnly)
{
	QHBoxLayout *layout = new QHBoxLayout;
	setLayout(layout);
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->insertStretch(0);
}

ActionLineEditButton * ActionLineEdit::widgetForAction ( QAction * action )
{
	QHBoxLayout *lo = (QHBoxLayout *)layout();
	ActionLineEditButton *btn;
	for (int i=1, count=lo->count(); i<count; i++) {
		btn = (ActionLineEditButton *)lo->itemAt(i)->widget();
		if (btn->defaultAction() == action) {
			return btn;
		}
	}
	return 0;
}

void ActionLineEdit::actionEvent ( QActionEvent * event )
{
	QHBoxLayout *lo = (QHBoxLayout *)layout();
	QAction *act = event->action();
	ActionLineEditButton *btn;
	if (event->type() == QEvent::ActionAdded) {
		btn = new ActionLineEditButton(this);
		QAction *before = event->before();
		int beforeInd;
		if (before && (beforeInd = actions().indexOf(before)) >= 0) { //TODO test it
			lo->insertWidget(beforeInd + 1, btn); //1 - first item is spacer. skip it
		}
		else {
			lo->addWidget(btn);
		}
		btn->setDefaultAction(act);
	}
	else if (event->type() == QEvent::ActionRemoved) {
		for (int i=1, count=lo->count(); i<count; i++) {
			btn = (ActionLineEditButton *)lo->itemAt(i)->widget();
			if (btn->defaultAction() == act) {
				lo->removeWidget(btn);
				delete btn;
				break;
			}
		}
	}
	int sumWidth = 0;
	for (int i=1, count=lo->count(); i<count; i++) {
		btn = (ActionLineEditButton *)lo->itemAt(i)->widget();
		if (btn->defaultAction()->isVisible()) {
			sumWidth += btn->width();
		}
	}
	sumWidth += 4; //+4px padding between text and buttons. should looks better (magic number)
#if QT_VERSION >= 0x040500
	int mLeft, mTop, mRight, mBottom;
	getTextMargins(&mLeft, &mTop, &mRight, &mBottom);
	setTextMargins(mLeft, mTop, sumWidth, mBottom);
#else
	setStyleSheet(QString("padding-right:%1px").arg(sumWidth)); //this breaks height of widget.. Qt bug?
#endif
}
