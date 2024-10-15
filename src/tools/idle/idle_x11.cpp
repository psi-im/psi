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
    struct {
        QString name    = QString();
        QString path    = QString();
        QString method  = QString();
        bool    isGnome = false;
    } idleService;
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
    const QStringList idleServices = { GNOME_SS_SERV, COMMON_SS_SERV, KDE_SS_SERV };
    // find first available dbus-service
    for (const auto &service : idleServices) {
        if (services.contains(service)) {
            bool isGnome   = (service == GNOME_SS_SERV);
            auto path      = isGnome ? GNOME_SS_PATH : COMMON_SS_PATH;
            auto interface = QDBusInterface(service, path, "org.freedesktop.DBus.Introspectable");
            if (interface.isValid()) {
                QDBusReply<QString> reply = interface.call("Introspect");
                if (reply.isValid()) {
                    if (reply.value().contains(isGnome ? GNOME_SS_F : COMMON_SS_F)) {
                        d->idleService.name    = service;
                        d->idleService.path    = path;
                        d->idleService.method  = isGnome ? GNOME_SS_F : COMMON_SS_F;
                        d->idleService.isGnome = isGnome;
                        return true;
                    }
                }
            }
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
    if (!d->idleService.name.isEmpty()) {
        // KDE and freedesktop uses the same path interface and method but gnome uses other
        auto interface = QDBusInterface(d->idleService.name, d->idleService.path, d->idleService.name);
        if (interface.isValid()) {
            QDBusMessage reply = interface.call(d->idleService.method);
            if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().count() > 0) {
                auto result = reply.arguments().at(0);
                if (result.canConvert<int>())
                    // reply should be in milliseconds and must be converted to seconds
                    return result.toInt() / 1000;
            }
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
