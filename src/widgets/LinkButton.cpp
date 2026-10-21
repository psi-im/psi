/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "LinkButton.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPainter>

LinkButton::LinkButton(QWidget *parent) : QAbstractButton(parent)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
}

QColor LinkButton::linkColor() const { return color_.isValid() ? color_ : palette().color(QPalette::Link); }

void LinkButton::setLinkColor(const QColor &color)
{
    color_ = color;
    update();
}

QSize LinkButton::sizeHint() const { return fontMetrics().size(Qt::TextShowMnemonic, text()); }

bool LinkButton::event(QEvent *event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
        hovered_ = event->type() == QEvent::Enter;
        update();
    } else if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        update();
    }
    return QAbstractButton::event(event);
}

void LinkButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setLayoutDirection(layoutDirection());
    painter.setPen(linkColor());
    painter.setOpacity(isEnabled() ? (hovered_ ? 1.0 : 0.8) : 0.4);
    QFont textFont = font();
    // Keep keyboard focus visible without asking the native button style to
    // paint a frame or start an animation.
    if (hasFocus())
        textFont.setUnderline(true);
    painter.setFont(textFont);
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, text());
}

void LinkButton::changeEvent(QEvent *event)
{
    QAbstractButton::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();
    update();
}

void LinkButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!event->isAutoRepeat())
            click();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void LinkButton::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    QAbstractButton::keyReleaseEvent(event);
}
