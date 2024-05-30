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

#include <memory>
#include <qglobal.h>

class QWidget;

class TaskBarNotifier {
public:
    explicit TaskBarNotifier(QWidget *parent = nullptr);
    ~TaskBarNotifier();
    void setIconCountCaption(int count);
    void removeIconCountCaption();
    bool isActive();
#ifdef Q_OS_WIN
    void enableFlashWindow(bool enabled);
#endif

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // TASKBARNOTIFIER_H
