/*
 * systemwatch.h - Detect changes in the system state.
 * Copyright (C) 2005  Remko Troncon
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

#include "systemwatch.h"

#if defined(Q_OS_MAC)
#include "systemwatch_mac.h"
#elif defined(Q_OS_WIN32)
#include "systemwatch_win.h"
#else
#include "systemwatch_unix.h"
#endif

#include <QApplication>

SystemWatch::SystemWatch() : QObject(qApp) { }

SystemWatch *SystemWatch::instance()
{
    if (!instance_) {
#if defined(Q_OS_MAC)
        instance_ = new MacSystemWatch();
#elif defined(Q_OS_WIN)
        instance_ = new WinSystemWatch();
#else
        instance_ = new UnixSystemWatch();
#endif
    }
    return instance_;
}

void SystemWatch::proceedWithSleep()
{
    // do nothing in base impl
}

SystemWatch *SystemWatch::instance_ = nullptr;
