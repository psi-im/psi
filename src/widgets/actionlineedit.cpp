/*
 * actionlineedit.h - QLineEdit widget with buttons on right side
 * Copyright (C) 2009  Sergey Ilinykh
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "actionlineedit.h"

#include <QAction>
#include <QActionEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QPainter>

ActionLineEditButton::ActionLineEditButton(QWidget *parent) : QAbstractButton(parent), action_(nullptr), popup_(nullptr)
{
    setCursor(Qt::PointingHandCursor);
}

/**
 * Same as for QToolButton
 */
void ActionLineEditButton::setDefaultAction(QAction *act)
{
    action_ = act;
    // TODO what if we set another action. disconnect events...
    connect(this, SIGNAL(clicked()), act, SLOT(trigger()));
    connect(act, SIGNAL(changed()), this, SLOT(updateFromAction()));

    updateFromAction();
}

QAction *ActionLineEditButton::defaultAction() { return action_; }

void ActionLineEditButton::setPopup(QWidget *widget)
{
    if (!popup_) {
        disconnect(this, SLOT(showPopup())); // TODO test it.
    }
    if (widget) {
        popup_ = widget;
        popup_->setWindowFlags(Qt::Popup);
        connect(action_, SIGNAL(triggered()), this, SLOT(showPopup()));
        popup_->adjustSize();
    }
}

void ActionLineEditButton::showPopup()
{
    if (popup_) {
        if (popup_->isHidden()) {
            const QPoint pos = mapToGlobal(QPoint(width() - popup_->width(), height()));
            popup_->move(pos);
            popup_->show();
        } else {
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

void ActionLineEditButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter            painter(this);
    ActionLineEdit *    p    = static_cast<ActionLineEdit *>(parent());
    Qt::ToolButtonStyle tbs  = p->toolButtonStyle();
    int                 lpos = 0;
    // int h = height();
    int h = p->minimumSizeHint().height() - 2; // 2px padding from max.
    if (!icon().isNull() && tbs != Qt::ToolButtonTextOnly) {
        const QPixmap pix = icon().pixmap(QSize(h, h), isEnabled() ? QIcon::Normal : QIcon::Disabled,
                                          isChecked() ? QIcon::On : QIcon::Off);
        painter.drawPixmap(0, (height() - pix.height()) / 2, pix);
        lpos += pix.width();
    }
    if (!(text().isEmpty()) && tbs != Qt::ToolButtonIconOnly) {
        if (lpos) {
            lpos += 2; // notice these 2px also in the sizeHint!
        }
        painter.drawText(QRect(lpos, 0, width() - lpos, height()), Qt::AlignVCenter, text());
    }
}

QSize ActionLineEditButton::sizeHint() const
{
    ActionLineEdit *    p   = static_cast<ActionLineEdit *>(parent());
    Qt::ToolButtonStyle tbs = p->toolButtonStyle();
    int                 w = 0, h = p->height();
    int                 ih = static_cast<QLineEdit *>(parent())->minimumSizeHint().height()
        - 2; // 2px padding from max. the same as in paintEvent
    QSize is(0, 0), ts(0, 0);
    if (!icon().isNull() && tbs != Qt::ToolButtonTextOnly) {
        is = icon().actualSize(QSize(ih, ih), isEnabled() ? QIcon::Normal : QIcon::Disabled,
                               isChecked() ? QIcon::On : QIcon::Off);
    }
    if (tbs != Qt::ToolButtonIconOnly) {
        ts = fontMetrics().size(Qt::TextSingleLine, text());
    }
    // 4px padding right, 2px -space between icon and text
    QSize ret = QSize(((w = ts.width()) > 0 ? (w + (is.width() ? 2 : 0)) : 0) + is.width() + 4, h);
    return ret;
}

ActionLineEdit::ActionLineEdit(QWidget *parent) : QLineEdit(parent), toolButtonStyle_(Qt::ToolButtonIconOnly)
{
    QHBoxLayout *layout = new QHBoxLayout;
    setLayout(layout);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->insertStretch(0);
}

ActionLineEditButton *ActionLineEdit::widgetForAction(QAction *action)
{
    QHBoxLayout *         lo = static_cast<QHBoxLayout *>(layout());
    ActionLineEditButton *btn;
    for (int i = 1, count = lo->count(); i < count; i++) {
        btn = static_cast<ActionLineEditButton *>(lo->itemAt(i)->widget());
        if (btn->defaultAction() == action) {
            return btn;
        }
    }
    return nullptr;
}

void ActionLineEdit::actionEvent(QActionEvent *event)
{
    QHBoxLayout *         lo  = static_cast<QHBoxLayout *>(layout());
    QAction *             act = event->action();
    ActionLineEditButton *btn;
    if (event->type() == QEvent::ActionAdded) {
        btn                = new ActionLineEditButton(this);
        QAction *before    = event->before();
        int      beforeInd = 0;
        if (before && (beforeInd = actions().indexOf(before)) >= 0) { // TODO test it
            lo->insertWidget(beforeInd + 1, btn);                     // 1 - first item is spacer. skip it
        } else {
            lo->addWidget(btn);
        }
        btn->setDefaultAction(act);
    } else if (event->type() == QEvent::ActionRemoved) {
        for (int i = 1, count = lo->count(); i < count; i++) {
            btn = static_cast<ActionLineEditButton *>(lo->itemAt(i)->widget());
            if (btn->defaultAction() == act) {
                lo->removeWidget(btn);
                delete btn;
                break;
            }
        }
    }
    int sumWidth = 0;
    for (int i = 1, count = lo->count(); i < count; i++) {
        btn = static_cast<ActionLineEditButton *>(lo->itemAt(i)->widget());
        if (btn->defaultAction()->isVisible()) {
            sumWidth += btn->width();
        }
    }
    sumWidth += 4; //+4px padding between text and buttons. should looks better (magic number)
    int mLeft, mTop, mRight, mBottom;
    getTextMargins(&mLeft, &mTop, &mRight, &mBottom);
    setTextMargins(mLeft, mTop, sumWidth, mBottom);
}

void ActionLineEdit::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu *menu = createStandardContextMenu();
    if (actions().count() > 0) {
        QAction *before = nullptr;
        if (menu->actions().count() > 0) {
            before = menu->actions().first();
        }
        menu->insertActions(before, actions());
        menu->insertSeparator(before);
    }
    menu->exec(e->globalPos());
    delete menu;
}
