/*
 * taskbarnotifier_win.cpp - Windows taskbar notifications class
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

#ifdef Q_OS_WINDOWS
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

class TaskBarNotifier::Private {
public:
    Private(TaskBarNotifier *base);

    void   setParentHWND(HWND hwnd) { hwnd_ = hwnd; };
    void   setUrgent(bool urgent) { urgent_ = urgent; };
    void   setParentIcon(const QImage &image, uint count);
    QImage makeIconCaption(const QImage &image, const QString &number);
    HICON  getHICONfromQImage(const QImage &image);
    void   doFlashTaskbarIcon();
    void   setDevicePixelRatio(int ratio) { devicePixelRatio_ = ratio; };

    TaskBarNotifier *tbn;

private:
    bool urgent_;
    HWND hwnd_;
    int  devicePixelRatio_;
};

TaskBarNotifier::Private::Private(TaskBarNotifier *base) : tbn(base), urgent_(false) { }

QImage TaskBarNotifier::Private::makeIconCaption(const QImage &image, const QString &number)
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

HICON TaskBarNotifier::Private::getHICONfromQImage(const QImage &image)
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

TaskBarNotifier::TaskBarNotifier(QWidget *parent, const QString &desktopfile) :
    count_(0), icon_(nullptr), active_(false)
{
    Q_UNUSED(desktopfile);
    d         = new Private(this);
    HWND hwnd = reinterpret_cast<HWND>(parent->winId());
    d->setParentHWND(hwnd);
    d->setDevicePixelRatio(parent->devicePixelRatio());
}

TaskBarNotifier::~TaskBarNotifier()
{
    delete icon_;
    delete d;
}

void TaskBarNotifier::setIconCounCaption(const QImage &icon, uint count)
{
    icon_ = new QImage(icon);
    d->setUrgent(true);
    d->setParentIcon(icon, count);
    active_ = true;
}

void TaskBarNotifier::removeIconCountCaption()
{
    d->setUrgent(false);
    d->setParentIcon(*icon_, 0);
    d->doFlashTaskbarIcon();
    active_ = false;
}
#endif
