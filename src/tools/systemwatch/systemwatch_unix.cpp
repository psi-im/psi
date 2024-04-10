/*
 * systemwatch_unix.h - Detect changes in the system state (Unix).
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

#include "systemwatch_unix.h"

#include "applicationinfo.h"

#ifdef USE_DBUS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#endif
#include <unistd.h>

UnixSystemWatch::UnixSystemWatch()
{
#ifdef USE_DBUS
    // TODO: check which service we should listen to
    QDBusConnection conn = QDBusConnection::systemBus();
    // listen to systemd's logind
    // TODO: use delaying Inhibitor locks
    conn.connect("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
                 "PrepareForSleep", this, SLOT(prepareForSleep(bool)));
    // listen to UPower
    conn.connect("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", "Sleeping", this,
                 SLOT(sleeping()));
    conn.connect("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", "Resuming", this,
                 SLOT(resuming()));

    takeSleepLock();
#endif
}

void UnixSystemWatch::takeSleepLock()
{
#ifdef USE_DBUS
    /* dbus-send --system --print-reply --dest=org.freedesktop.login1 /org/freedesktop/login1 \
       "org.freedesktop.login1.Manager.Inhibit" string:"sleep" string:"Psi" \
                                                               string:"Closing connections..." string:"delay"
    */
    QDBusInterface login1iface("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
                               QDBusConnection::systemBus());
    QDBusReply<QDBusUnixFileDescriptor> repl
        = login1iface.call("Inhibit", "sleep", ApplicationInfo::name(), "Closing connections...", "delay");
    // we could delay "shutdown" as well probably Qt session manager already can do this
    if (repl.isValid()) {
        lockFd = repl.value();
    } else {
        lockFd = QDBusUnixFileDescriptor();
    }
#endif
}

void UnixSystemWatch::proceedWithSleep()
{
#ifdef USE_DBUS
    lockFd = QDBusUnixFileDescriptor(); // null descriptor should release it
#endif
}

void UnixSystemWatch::prepareForSleep(bool beforeSleep)
{
    if (beforeSleep) {
        emit sleep();
    } else {
        emit wakeup();
        takeSleepLock();
    }
}

void UnixSystemWatch::sleeping() { emit sleep(); }

void UnixSystemWatch::resuming() { emit wakeup(); }
