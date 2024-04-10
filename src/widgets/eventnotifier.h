/*
 * eventnotifier.h - Notification frame for events
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

#ifndef EVENTNOTIFIER_H
#define EVENTNOTIFIER_H

#include "iconlabel.h"
#include "iconset.h"
#include <QWidget>

class QLabel;
class QMouseEvent;

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    ClickableLabel(QWidget *parent = nullptr);

protected:
    // reimplemented
    void mouseReleaseEvent(QMouseEvent *);

signals:
    void clicked(int);
};

class EventNotifier : public QFrame {
    Q_OBJECT
public:
    explicit EventNotifier(QWidget *parent = nullptr, const char *name = nullptr);
    void setPsiIcon(PsiIcon *icon);
    void setPsiIcon(const QString &name);
    void setText(const QString &text);

signals:
    void clicked(int);
    void clearEventQueue();

protected:
    void mouseReleaseEvent(QMouseEvent *);
    void contextMenuEvent(QContextMenuEvent *e);

private:
    IconLabel      *eventIcon;
    ClickableLabel *eventLabel;
};

#endif // EVENTNOTIFIER_H
