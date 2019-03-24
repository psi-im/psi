/*
 * debug.h - debug staff
 * Copyright (C) 2017  Ivan Romanov <drizt@land.ru>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <QElapsedTimer>
#include <QDebug>

// history

# define EDB_DEBUG() qDebug().noquote() << "[edb]"
# define EDB_CRITICAL() qCritical().noquote() << "[edb]"
# define EDB_WARNING() qWarning().noquote() << "[edb]"
# define EDB_FATAL() QDebug(QtMsgType::QtFatalMsg).noquote() << "[edb]"

// contact list

# define CL_DEBUG() qDebug().noquote() << "[cl]"
# define CL_CRITICAL() qCritical().noquote() << "[cl]"
# define CL_WARNING() qWarning().noquote() << "[cl]"
# define CL_FATAL() QDebug(QtMsgType::QtFatalMsg).noquote() << "[cl]"

// Common

# define DEBUG() qDebug().noquote()
# define CRITICAL() qCritical().noquote()
# define WARNING() qWarning().noquote()
# define FATAL() QDebug(QtMsgType::QtFatalMsg).noquote()

class SlowTimer
{
public:
    SlowTimer(const QString &path, int line, int maxTime = 0, const QString &message = QString());
    ~SlowTimer();

private:
    QElapsedTimer _timer;
    QString _path;
    int _line;
    QString _message;
    int _maxTime;
};

#define SLOW_TIMER(...) SlowTimer slowTimer(__FILE__, __LINE__, __VA_ARGS__)
