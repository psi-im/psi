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

#include <QWidget>

#ifdef USE_DBUS
#include "applicationinfo.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#endif
#ifdef Q_OS_WIN
#include <QImage>
#include <QPaintDevice>
#include <QPainter>
#include <QPen>
#include <shobjidl.h>
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
    Private() = default;

    bool active() const;
    void setIconCount(uint count = 0);
    void restoreDefaultIcon();
#ifdef USE_DBUS
    ~Private() = default;
    void setDesktopPath(const QString &appName);
#elif defined(Q_OS_WIN)
    ~Private();
    void setParentHWND(HWND hwnd);
    void setDevicePixelRatio(int ratio);
#endif

private:
#ifdef USE_DBUS
    bool checkDBusSeviceAvailable();
    void sendDBusSignal(bool isVisible, uint number = 0);
#elif defined(Q_OS_WIN)
    void  setTaskBarIcon(const HICON &icon = {});
    HICON makeIconCaption(const QString &number) const;
    HICON getHICONfromQImage(const QImage &image) const;
    void  doFlashTaskbarIcon();
#endif

private:
    bool urgent_ = false;
    bool active_ = false;
#ifdef Q_OS_WIN
    HWND  hwnd_;
    int   devicePixelRatio_;
    HICON icon_;
#endif
};

bool TaskBarNotifier::Private::active() const { return active_; }

void TaskBarNotifier::Private::setIconCount(uint count)
{
    urgent_ = true;
#ifdef USE_DBUS
    sendDBusSignal(true, count);
#elif defined(Q_OS_WIN)
    setTaskBarIcon(makeIconCaption(QString::number(count)));
    doFlashTaskbarIcon();
#endif
    active_ = true;
}

void TaskBarNotifier::Private::restoreDefaultIcon()
{
    urgent_ = false;
#ifdef USE_DBUS
    sendDBusSignal(false, 0);
#elif defined(Q_OS_WIN)
    setTaskBarIcon();
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

#elif defined(Q_OS_WIN)
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

    if (icon)
        icon_ = icon;

    ITaskbarList3 *tbList;
    if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
                                   reinterpret_cast<void **>(&tbList)))) {
        tbList->SetOverlayIcon(hwnd_, icon_, (icon_) ? L"Incoming events" : 0);
        tbList->Release();
    }
}

HICON TaskBarNotifier::Private::makeIconCaption(const QString &number) const
{
    auto   imSize = QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)) * devicePixelRatio_;
    QImage img(imSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QString text      = (number.length() < 3) ? number : "∞";
    auto    letters   = text.length();
    auto    textDelta = (letters <= 2) ? 2 : 3;

    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    auto font   = QFont("Times", imSize.height() / textDelta, QFont::Bold);
    auto fm     = QFontMetrics(font);
    auto fh     = fm.height();
    auto radius = fh / 2;

    Qt::BrushStyle style = Qt::SolidPattern;
    QBrush         brush(Qt::black, style);
    p.setBrush(brush);
    p.setPen(QPen(Qt::yellow));
    QRect rect(0, 0, imSize.width() - 2, imSize.height() - 2);
    p.drawRoundedRect(rect, radius, radius);

    p.setFont(font);
    p.setPen(QPen(Qt::white));
    p.drawText(rect, Qt::AlignCenter, text);

    p.end();
    return std::move(getHICONfromQImage(img));
}

HICON TaskBarNotifier::Private::getHICONfromQImage(const QImage &image) const
{
    if (image.isNull())
        return {};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return std::move(QtWin::toHICON(QPixmap::fromImage(image)));
#else
    return std::move(image.toHICON());
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
#endif

TaskBarNotifier::TaskBarNotifier(QWidget *parent)
{
    d = std::make_unique<Private>(Private());
#ifdef USE_DBUS
    Q_UNUSED(parent)
#elif defined(Q_OS_WIN)
    d->setParentHWND(reinterpret_cast<HWND>(parent->winId()));
    d->setDevicePixelRatio(parent->devicePixelRatio());
#endif
}

TaskBarNotifier::~TaskBarNotifier() = default;

void TaskBarNotifier::setIconCountCaption(int count) { d->setIconCount(count); }

void TaskBarNotifier::removeIconCountCaption() { d->restoreDefaultIcon(); }

bool TaskBarNotifier::isActive() { return d->active(); }
