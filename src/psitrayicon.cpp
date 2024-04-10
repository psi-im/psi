#include "psitrayicon.h"

#include "alerticon.h"
#include "common.h"
#include "iconset.h"

#include <QHelpEvent>
#include <QPixmap>
#include <QPixmapCache>
#include <QSystemTrayIcon>

// TODO: remove the QPoint parameter from the signals when we finally move
// to the new system.

PsiTrayIcon::PsiTrayIcon(const QString &tip, QMenu *popup, QObject *parent) :
    QObject(parent), icon_(nullptr), trayicon_(new QSystemTrayIcon()), realIcon_(0)
{
    trayicon_->setContextMenu(popup);
    setToolTip(tip);
    connect(trayicon_, &QSystemTrayIcon::activated, this, &PsiTrayIcon::trayicon_activated);
    trayicon_->installEventFilter(this);
}

PsiTrayIcon::~PsiTrayIcon()
{
    delete trayicon_;
    if (icon_) {
        icon_->disconnect();
        icon_->stop();
        delete icon_;
    }
}

void PsiTrayIcon::setContextMenu(QMenu *menu) { trayicon_->setContextMenu(menu); }

void PsiTrayIcon::setToolTip(const QString &str) { trayicon_->setToolTip(str); }

void PsiTrayIcon::setIcon(const PsiIcon *icon, bool alert)
{
    if (icon_) {
        icon_->disconnect();
        icon_->stop();

        delete icon_;
        icon_ = nullptr;
    }

    realIcon_ = quintptr(icon);
    if (icon) {
        if (!alert)
            icon_ = new PsiIcon(*icon);
        else
            icon_ = new AlertIcon(icon);

        connect(icon_, &PsiIcon::pixmapChanged, this, &PsiTrayIcon::animate);
        icon_->activated();
    } else
        icon_ = new PsiIcon();

    animate();
}

void PsiTrayIcon::setAlert(const PsiIcon *icon) { setIcon(icon, true); }

void PsiTrayIcon::show() { trayicon_->show(); }

void PsiTrayIcon::hide() { trayicon_->hide(); }

QPixmap PsiTrayIcon::makeIcon()
{
    if (!icon_)
        return QPixmap();

    return icon_->pixmap(trayicon_->geometry().size());
}

void PsiTrayIcon::trayicon_activated(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MAC
    Q_UNUSED(reason)
#else
    if (reason == QSystemTrayIcon::Trigger)
        emit clicked(QPoint(), Qt::LeftButton);
    else if (reason == QSystemTrayIcon::MiddleClick || (isKde() && reason == QSystemTrayIcon::Context))
        emit clicked(QPoint(), Qt::MiddleButton);
#ifdef Q_OS_WIN
    else if (reason == QSystemTrayIcon::DoubleClick)
        emit doubleClicked(QPoint());
#endif
#endif
}

void PsiTrayIcon::animate()
{
    if (!icon_)
        return;

    QString cachedName = QString("PsiTray/%1/%2/%3")
                             .arg(icon_->name(), QString::number(realIcon_), QString::number(icon_->frameNumber()));

    QPixmap p;
    if (!QPixmapCache::find(cachedName, &p)) {
        p = makeIcon();
        QPixmapCache::insert(cachedName, p);
    }
    trayicon_->setIcon(p);
}

bool PsiTrayIcon::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == trayicon_ && event->type() == QEvent::ToolTip) {
        emit doToolTip(obj, (static_cast<QHelpEvent *>(event))->globalPos());
        return true;
    }
    return QObject::eventFilter(obj, event);
}
