/*
 * taskbarnotifier.cpp - Taskbar notifications class
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
#include "applicationinfo.h"

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
#include <windows.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWinExtras/qwinfunctions.h>
#endif
#endif
#include <QWidget>

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
    Private() = default;

    bool active() const;
    void setIconCount(uint count = 0);
    void restoreDefaultIcon();
#ifdef USE_DBUS
    ~Private() = default;
    void setDesktopPath(const QString &appName);
#elif defined(Q_OS_WINDOWS)
    ~Private();
    void setParentHWND(HWND hwnd);
    void setDevicePixelRatio(int ratio);
    void setDefaultIcon(const QImage &icon);
#endif

private:
#ifdef USE_DBUS
    bool checkDBusSeviceAvailable();
    void sendDBusSignal(bool isVisible, uint number = 0);
#elif defined(Q_OS_WINDOWS)
    void   setTaskBarIcon(const HICON &icon);
    QImage makeIconCaption(const QImage &image, const QString &number) const;
    HICON  getHICONfromQImage(const QImage &image) const;
    void   doFlashTaskbarIcon();
#endif

private:
    bool urgent_ = false;
    bool active_ = false;
#ifdef Q_OS_WINDOWS
    HWND   hwnd_;
    int    devicePixelRatio_;
    QImage image_;
    HICON  icon_;
#endif
};

bool TaskBarNotifier::Private::active() const { return active_; }

void TaskBarNotifier::Private::setIconCount(uint count)
{
    urgent_ = true;
#ifdef USE_DBUS
    sendDBusSignal(true, count);
#elif defined(Q_OS_WINDOWS)
    if (image_.isNull())
        return;

    QImage img;
    if (count > 0 && urgent_) {
        img = makeIconCaption(image_, QString::number(count));
    }
    HICON icon = (img.isNull()) ? getHICONfromQImage(image_) : getHICONfromQImage(img);
    setTaskBarIcon(icon);
    doFlashTaskbarIcon();
#endif
    active_ = true;
}

void TaskBarNotifier::Private::restoreDefaultIcon()
{
    urgent_ = false;
#ifdef USE_DBUS
    sendDBusSignal(false, 0);
#elif defined(Q_OS_WINDOWS)
    if (image_.isNull())
        return;

    HICON icon = getHICONfromQImage(image_);
    setTaskBarIcon(icon);
    doFlashTaskbarIcon();
#endif
    active_ = false;
}

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
    if (checkDBusSeviceAvailable()) {
        auto appName = ApplicationInfo::desktopFileBaseName();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        auto desktopPath_ = QLatin1String("application://%1").arg(appName);
#else
        auto desktopPath_ = QLatin1String("application://%1.desktop").arg(appName);
#endif
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

#elif defined(Q_OS_WINDOWS)
TaskBarNotifier::Private::~Private()
{
    if (icon_)
        DestroyIcon(icon_);
}

void TaskBarNotifier::Private::setParentHWND(HWND hwnd) { hwnd_ = hwnd; }

void TaskBarNotifier::Private::setDevicePixelRatio(int ratio) { devicePixelRatio_ = ratio; }

void TaskBarNotifier::Private::setTaskBarIcon(const HICON &icon)
{
    if (icon_)
        DestroyIcon(icon_);

    icon_ = icon;
    SendMessage(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)icon_);
    SendMessage(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)icon_);
}

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

HICON TaskBarNotifier::Private::getHICONfromQImage(const QImage &image) const
{
    if (image.isNull())
        return {};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto p = QPixmap::fromImage(image);
    return QtWin::toHICON(p);
#else
    return image.toHICON();
#endif
    return {};
}

void TaskBarNotifier::Private::doFlashTaskbarIcon()
{
    FLASHWINFO fi;
    fi.cbSize    = sizeof(FLASHWINFO);
    fi.hwnd      = hwnd_;
    fi.dwFlags   = (urgent_) ? FLASHW_ALL | FLASHW_TIMER : FLASHW_STOP;
    fi.uCount    = 0;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
}

void TaskBarNotifier::Private::setDefaultIcon(const QImage &icon) { image_ = icon; }
#endif

TaskBarNotifier::TaskBarNotifier(QWidget *parent)
{
    d = std::make_unique<Private>(Private());
#ifdef USE_DBUS
    Q_UNUSED(parent)
#elif defined(Q_OS_WINDOWS)
    HWND hwnd = reinterpret_cast<HWND>(parent->winId());
    d->setParentHWND(hwnd);
    d->setDevicePixelRatio(parent->devicePixelRatio());
#endif
}

TaskBarNotifier::~TaskBarNotifier() = default;

void TaskBarNotifier::setIconCountCaption(uint count, const QImage &icon)
{
#ifdef Q_OS_WINDOWS
    d->setDefaultIcon(icon);
#else
    Q_UNUSED(icon);
#endif
    d->setIconCount(count);
}

void TaskBarNotifier::removeIconCountCaption() { d->restoreDefaultIcon(); }

bool TaskBarNotifier::isActive() { return d->active(); }
