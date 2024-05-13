/*
 * taskbarnotifier.h - Taskbar notifications class
 * Copyright (C) 2024  Vitaly Tonkacheyev
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

#ifndef TASKBARNOTIFIER_H
#define TASKBARNOTIFIER_H

#include <QWidget>

class QString;
class QImage;

class TaskBarNotifier {
public:
    explicit TaskBarNotifier(QWidget *parent = nullptr, const QString &desktopfile = nullptr);
    ~TaskBarNotifier();
    void setIconCounCaption(const QImage &icon, uint count);
    void removeIconCountCaption();
    bool isActive() { return active_; };

private:
    class Private;
    Private *d;
    uint     count_;
    QImage  *icon_;
    bool     active_;
};

#endif // TASKBARNOTIFIER_H
