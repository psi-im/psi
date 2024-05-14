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
#elif defined(Q_OS_WINDOWS)
#include <QImage>
#include <QPaintDevice>
#include <QPainter>
#include <QPen>
#include <QStaticText>
#include <QWidget>
#include <windows.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWinExtras/qwinfunctions.h>
#endif
#endif

#ifdef USE_DBUS
// UnityLauncher dbus
static const QLatin1String ULAUNCHER_SERV("com.canonical.Unity");
static const QLatin1String ULAUNCHER_PATH("/");
static const QLatin1String ULAUNCHER_IFACE("com.canonical.Unity.LauncherEntry");
// UnityLauncher functions
static const QLatin1String ULAUNCHER_CMD("Update");
#endif

class TaskBarNotifier::Private {
public:
    Private(TaskBarNotifier *);

    void setUrgent(bool urgent) { urgent_ = urgent; };
#ifdef USE_DBUS
    bool checkDBusSeviceAvailable();
    void sendDBusSignal(bool isVisible, uint number = 0);
    void setDesktopPath(const QString &appName);
#elif defined(Q_OS_WINDOWS)
    void   setParentHWND(HWND hwnd) { hwnd_ = hwnd; };
    void   setParentIcon(const QImage &image, uint count);
    QImage makeIconCaption(const QImage &image, const QString &number) const;
    HICON  getHICONfromQImage(const QImage &image) const;
    void   doFlashTaskbarIcon();
    void   setDevicePixelRatio(int ratio) { devicePixelRatio_ = ratio; };
#endif

private:
    bool urgent_;
#ifdef USE_DBUS
    bool    isServiceAvailable_;
    QString desktopPath_;
#elif defined(Q_OS_WINDOWS)
    HWND hwnd_;
    int  devicePixelRatio_;
#endif
};

TaskBarNotifier::Private::Private(TaskBarNotifier *) :
    urgent_(false)
#ifdef USE_DBUS
    ,
    isServiceAvailable_(false), desktopPath_(QLatin1String())
{
    isServiceAvailable_ = checkDBusSeviceAvailable();
}
#elif defined(Q_OS_WINDOWS)
{
}
#endif

#ifdef USE_DBUS
bool TaskBarNotifier::Private::checkDBusSeviceAvailable()
{
    const auto services = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    for (const auto &service : services) {
        if (service.contains(ULAUNCHER_SERV, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void TaskBarNotifier::Private::sendDBusSignal(bool isVisible, uint number)
{
    if (isServiceAvailable_) {
        QDBusMessage signal = QDBusMessage::createSignal(ULAUNCHER_PATH, ULAUNCHER_IFACE, ULAUNCHER_CMD);
        signal << desktopPath_;
        QVariantMap args;
        args["count-visible"] = isVisible;
        args["count"]         = number;
        args["urgent"]        = urgent_;
        signal << args;
        QDBusConnection::sessionBus().send(signal);
    }
}

void TaskBarNotifier::Private::setDesktopPath(const QString &appName)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    desktopPath_ = QLatin1String("application://%1").arg(appName);
#else
    desktopPath_ = QLatin1String("application://%1.desktop").arg(appName);
#endif
}

#elif defined(Q_OS_WINDOWS)
QImage TaskBarNotifier::Private::makeIconCaption(const QImage &image, const QString &number) const
{
    if (!image.isNull()) {
        auto   imSize    = image.size() * devicePixelRatio_;
        QImage img       = image;
        auto   letters   = number.length();
        auto   text      = QStaticText((letters < 3) ? number : "99+");
        auto   textDelta = (letters <= 2) ? 3 : 4;

        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        auto font   = QFont("Times", imSize.height() / textDelta, QFont::Bold);
        auto fm     = QFontMetrics(font);
        auto fh     = fm.height();
        auto fw     = fm.horizontalAdvance(text.text());
        auto radius = fh / 2;

        Qt::BrushStyle style = Qt::SolidPattern;
        QBrush         brush(Qt::red, style);
        p.setBrush(brush);
        p.setPen(QPen(Qt::NoPen));
        QRect rect(imSize.width() - fw - radius, 0, fw + radius, fh);
        p.drawRoundedRect(rect, radius, radius);

        p.setFont(font);
        p.setPen(QPen(Qt::white));
        auto offset = rect.width() / ((letters + 1) * 2);
        p.drawStaticText(rect.x() + offset, rect.y(), text);

        p.end();
        return img;
    }
    return QImage();
}

void TaskBarNotifier::Private::setParentIcon(const QImage &image, uint count)
{
    if (image.isNull())
        return;

    QImage img;
    if (count > 0 && urgent_) {
        img = makeIconCaption(image, QString::number(count));
    }
    HICON icon = (img.isNull()) ? getHICONfromQImage(image) : getHICONfromQImage(img);
    SendMessage(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    SendMessage(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)icon);
    doFlashTaskbarIcon();
}

HICON TaskBarNotifier::Private::getHICONfromQImage(const QImage &image) const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto p = QPixmap::fromImage(image);
    return QtWin::toHICON(p);
#else
    return image.toHICON();
#endif
}

void TaskBarNotifier::Private::doFlashTaskbarIcon()
{
    const int count = 0;
    const int speed = 0;

    FLASHWINFO fi;
    fi.cbSize    = sizeof(FLASHWINFO);
    fi.hwnd      = hwnd_;
    fi.dwFlags   = (urgent_) ? FLASHW_ALL | FLASHW_TIMER : FLASHW_STOP;
    fi.uCount    = count;
    fi.dwTimeout = speed;
    FlashWindowEx(&fi);
}
#endif

TaskBarNotifier::TaskBarNotifier(QWidget *parent, const QString &desktopfile) :
    count_(0), icon_(nullptr), active_(false)
{
    d = new Private(this);
#ifdef USE_DBUS
    Q_UNUSED(parent)
    d->setDesktopPath(desktopfile);
#elif defined(Q_OS_WINDOWS)
    Q_UNUSED(desktopfile);
    HWND hwnd = reinterpret_cast<HWND>(parent->winId());
    d->setParentHWND(hwnd);
    d->setDevicePixelRatio(parent->devicePixelRatio());
#endif
}

TaskBarNotifier::~TaskBarNotifier()
{
    if (icon_)
        delete icon_;
    delete d;
}

void TaskBarNotifier::setIconCounCaption(const QImage &icon, uint count)
{
    d->setUrgent(true);
#ifdef USE_DBUS
    Q_UNUSED(icon);
    d->sendDBusSignal(true, count);
#elif defined(Q_OS_WINDOWS)
    icon_ = new QImage(icon);
    d->setUrgent(true);
    d->setParentIcon(icon, count);
    active_ = true;
#endif
    active_ = true;
}

void TaskBarNotifier::removeIconCountCaption()
{
    d->setUrgent(false);
#ifdef USE_DBUS
    d->sendDBusSignal(false, 0);
#elif defined(Q_OS_WINDOWS)
    d->setParentIcon(*icon_, 0);
    d->doFlashTaskbarIcon();
#endif
    active_ = false;
}
