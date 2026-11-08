#include "psitrayicon.h"

#include "common.h"
#include "iconset.h"
#include "TrayBadgePainter.h"

#include <QHelpEvent>
#include <QPixmap>
#include <QIcon>
#include <QSystemTrayIcon>

// TODO: remove the QPoint parameter from the signals when we finally move
// to the new system.

PsiTrayIcon::PsiTrayIcon(const QString &tip, QMenu *popup, QObject *parent) :
    QObject(parent), icon_(nullptr), trayicon_(new QSystemTrayIcon())
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

void PsiTrayIcon::setIcon(const PsiIcon *icon)
{
    // Unread changes must not repeatedly restart a status animation or
    // replace the native tray icon twice (status followed by badge).
    if (icon_ && sourceIcon_.data() == icon)
        return;
    sourceIcon_ = const_cast<PsiIcon *>(icon);
    if (icon_) {
        icon_->disconnect();
        icon_->stop();
        delete icon_;
        icon_ = nullptr;
    }

    if (icon) {
        icon_ = new PsiIcon(*icon);
        connect(icon_, &PsiIcon::pixmapChanged, this, &PsiTrayIcon::animate);
        icon_->activated();
    } else {
        icon_ = new PsiIcon();
    }
    animate();
}

void PsiTrayIcon::setBadge(int attentionCount, bool unreadMuc)
{
    attentionCount = qMax(0, attentionCount);
    const auto oldKind = Mattermost::trayBadgeKind(static_cast<std::uint32_t>(attentionCount_), unreadMuc_);
    const auto oldDisplayCount = qMin(attentionCount_, 100);
    const auto newKind = Mattermost::trayBadgeKind(static_cast<std::uint32_t>(attentionCount), unreadMuc);
    attentionCount_ = attentionCount;
    unreadMuc_ = unreadMuc;
    // The blue state may change behind a red badge. Neither that nor a
    // change from 100 to 101 affects the native icon pixels.
    if (oldKind == newKind && (oldKind != Mattermost::TrayBadgeKind::Attention
                               || oldDisplayCount == qMin(attentionCount_, 100)))
        return;
    animate();
}

void PsiTrayIcon::show() { trayicon_->show(); }

void PsiTrayIcon::hide() { trayicon_->hide(); }

void PsiTrayIcon::trayicon_activated(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MACOS
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

    // Render all common native tray sizes, rather than caching a single
    // pixmap with a stale badge or depending on tray geometry during startup.
    const auto kind = Mattermost::trayBadgeKind(static_cast<std::uint32_t>(attentionCount_), unreadMuc_);
    QIcon composed;
    for (int edge : { 16, 22, 24, 32, 48, 64, 128, 256 }) {
        QPixmap pixmap = icon_->pixmap(QSize(edge, edge));
        if (pixmap.isNull())
            continue;
        Mattermost::TrayBadgePainter::paint(pixmap, kind, static_cast<std::uint32_t>(attentionCount_));
        composed.addPixmap(pixmap);
    }
    if (!composed.isNull())
        trayicon_->setIcon(composed);
}

bool PsiTrayIcon::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == trayicon_ && event->type() == QEvent::ToolTip) {
        emit doToolTip(obj, (static_cast<QHelpEvent *>(event))->globalPos());
        return true;
    }
    return QObject::eventFilter(obj, event);
}
