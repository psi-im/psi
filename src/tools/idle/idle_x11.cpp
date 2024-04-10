/*
 * idle_x11.cpp - detect desktop idle time
 * Copyright (C) 2003  Justin Karneges
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

#include "idle.h"

#ifdef HAVE_XSS
#include "x11windowsystem.h"
#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

static XErrorHandler old_handler = 0;
extern "C" int       xerrhandler(Display *dpy, XErrorEvent *err)
{
    if (err->error_code == BadDrawable)
        return 0;

    return (*old_handler)(dpy, err);
}

auto getDisplay()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QX11Info::display();
#else
    auto x11app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11app->display();
#endif
}
#endif // HAVE_XSS

#ifdef USE_DBUS

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

// Screen Saver dbus services
static const QLatin1String COMMON_SS_SERV("org.freedesktop.ScreenSaver");
static const QLatin1String COMMON_SS_PATH("/ScreenSaver");
static const QLatin1String KDE_SS_SERV("org.kde.screensaver");
static const QLatin1String GNOME_SS_SERV("org.gnome.Mutter.IdleMonitor");
static const QLatin1String GNOME_SS_PATH("/org/gnome/Mutter/IdleMonitor/Core");
// Screen saver functions
static const QLatin1String GNOME_SS_F("GetIdletime");
static const QLatin1String COMMON_SS_F("GetSessionIdleTime");

#endif // USE_DBUS

class IdlePlatform::Private {
public:
    Private() { }

#ifdef USE_DBUS
    QString dbusService = QString();
#endif // USE_DBUS
#ifdef HAVE_XSS
    XScreenSaverInfo *ss_info = nullptr;
#endif // HAVE_XSS
};

IdlePlatform::IdlePlatform()
{
#if defined(HAVE_XSS) || defined(USE_DBUS)
    d = new Private;
#else
    d = nullptr;
#endif
}

IdlePlatform::~IdlePlatform()
{
#ifdef HAVE_XSS
    if (d->ss_info)
        XFree(d->ss_info);
    if (old_handler) {
        XSetErrorHandler(old_handler);
        old_handler = 0;
    }
#endif // HAVE_XSS
    if (d)
        delete d;
}

bool IdlePlatform::init()
{
#ifdef USE_DBUS
    // if DBUS idle is available using it else try to use XSS functions
    const auto        services     = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    const QStringList idleServices = { COMMON_SS_SERV, KDE_SS_SERV, GNOME_SS_SERV };
    // find first available dbus-service
    for (const auto &service : idleServices) {
        if (services.contains(service)) {
            d->dbusService = service;
            return true;
        }
    }
#endif // USE_DBUS
#ifdef HAVE_XSS
    if (!X11WindowSystem::instance()->isValid())
        return false;

    if (d->ss_info)
        return true;

    old_handler = XSetErrorHandler(xerrhandler);

    int event_base, error_base;
    if (XScreenSaverQueryExtension(getDisplay(), &event_base, &error_base)) {
        d->ss_info = XScreenSaverAllocInfo();
        return true;
    }
#endif // HAVE_XSS
    return false;
}

int IdlePlatform::secondsIdle()
{
#ifdef USE_DBUS
    if (!d->dbusService.isEmpty()) {
        // KDE and freedesktop uses the same path interface and method but gnome uses other
        bool                isNotGnome = d->dbusService == COMMON_SS_SERV || d->dbusService == KDE_SS_SERV;
        const QLatin1String iface      = isNotGnome ? COMMON_SS_SERV : GNOME_SS_SERV;
        const QLatin1String path       = isNotGnome ? COMMON_SS_PATH : GNOME_SS_PATH;
        const QLatin1String method     = isNotGnome ? COMMON_SS_F : GNOME_SS_F;
        auto                interface  = QDBusInterface(d->dbusService, path, iface);
        if (interface.isValid()) {
            QDBusReply<uint> reply = interface.call(method);
            // probably reply value for freedesktop and kde need to be converted to seconds
            if (reply.isValid())
                return isNotGnome ? reply.value() / 1000 : reply.value();
        }
    }
#endif // USE_DBUS
#ifdef HAVE_XSS
    if (!d->ss_info)
        return 0;

    if (!XScreenSaverQueryInfo(getDisplay(), X11WindowSystem::instance()->getDesktopRootWindow(), d->ss_info))
        return 0;
    return d->ss_info->idle / 1000;
#endif // HAVE_XSS
    return 0;
}
