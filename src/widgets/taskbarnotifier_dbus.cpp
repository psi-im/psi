/*
 * taskbarnotifier_dbus.cpp - DBUS taskbar notifications class
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

#include "taskbarnotifier.h"

#ifdef USE_DBUS
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// UnityLauncher dbus
static const QLatin1String ULAUNCHER_SERV("com.canonical.Unity");
static const QLatin1String ULAUNCHER_PATH("/");
static const QLatin1String ULAUNCHER_IFACE("com.canonical.Unity.LauncherEntry");
// UnityLauncher functions
static const QLatin1String ULAUNCHER_CMD("Update");

class TaskBarNotifier::Private {
public:
    Private(TaskBarNotifier *tn = nullptr);

    bool        checkDBusSeviceAvailable();
    inline void setUrgent(bool urgent) { urgent_ = urgent; };
    bool        sendDBusSignal(bool isVisible, uint number = 0);
    void        setDesktopPath(const QString &appName);

    TaskBarNotifier *tbn;

private:
    bool    isServiceAvailable_;
    bool    urgent_;
    QString desktopPath_;
};

TaskBarNotifier::Private::Private(TaskBarNotifier *tn) :
    tbn(tn), isServiceAvailable_(false), urgent_(false), desktopPath_(QLatin1String())
{
    isServiceAvailable_ = checkDBusSeviceAvailable();
}

bool TaskBarNotifier::Private::checkDBusSeviceAvailable()
{
    const auto services = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    for (const auto &service : services) {
        if (service.contains(ULAUNCHER_SERV, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool TaskBarNotifier::Private::sendDBusSignal(bool isVisible, uint number)
{
    bool done = false;
    if (isServiceAvailable_) {
        QDBusMessage signal = QDBusMessage::createSignal(ULAUNCHER_PATH, ULAUNCHER_IFACE, ULAUNCHER_CMD);
        signal << desktopPath_;
        QVariantMap args;
        args["count-visible"] = isVisible;
        args["count"]         = number;
        args["urgent"]        = urgent_;
        signal << args;
        done = QDBusConnection::sessionBus().send(signal);
    }
    return done;
}

void TaskBarNotifier::Private::setDesktopPath(const QString &appName)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    desktopPath_ = QLatin1String("application://%1").arg(appName);
#else
    desktopPath_ = QLatin1String("application://%1.desktop").arg(appName);
#endif
}

TaskBarNotifier::TaskBarNotifier(QWidget *parent, const QString &desktopfile) :
    count_(0), icon_(nullptr), active_(false)
{
    Q_UNUSED(parent)
    d = new Private(this);
    d->setDesktopPath(desktopfile);
}

TaskBarNotifier::~TaskBarNotifier() { delete d; }

void TaskBarNotifier::setIconCounCaption(const QImage &icon, uint count)
{
    Q_UNUSED(icon);
    d->setUrgent(true);
    d->sendDBusSignal(true, count);
    active_ = true;
}

void TaskBarNotifier::removeIconCountCaption()
{
    d->setUrgent(false);
    d->sendDBusSignal(false, 0);
    active_ = false;
}
#endif
