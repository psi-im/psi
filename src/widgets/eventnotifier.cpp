/*
 * eventnotifier.cpp - Notification frame for events
 * Copyright (C) 2022  Vitaly Tonkacheyev
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

#include "eventnotifier.h"
#include <QHBoxLayout>

ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent)
{
    setMinimumWidth(48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *e)
{
    emit clicked(int(e->button()));
    e->ignore();
}

EventNotifier::EventNotifier(QWidget *parent, const char *name) :
    QFrame { parent }, eventIcon(new IconLabel(this)), eventLabel(new ClickableLabel(this))
{
    setObjectName(name);
    QHBoxLayout *eventLayout = new QHBoxLayout(this);
    eventLayout->addWidget(eventIcon);
    eventLayout->addWidget(eventLabel);
    eventLabel->setText("");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFrameStyle(QFrame::Panel | QFrame::Sunken);
    connect(eventLabel, &ClickableLabel::clicked, this, &EventNotifier::clicked);
}

void EventNotifier::mouseReleaseEvent(QMouseEvent *e)
{
    emit clicked(int(e->button()));
    e->ignore();
}

void EventNotifier::setPsiIcon(PsiIcon *icon) { eventIcon->setPsiIcon(icon); }

void EventNotifier::setPsiIcon(const QString &name) { eventIcon->setPsiIcon(name); }

void EventNotifier::setText(const QString &text) { eventLabel->setText(text); }
