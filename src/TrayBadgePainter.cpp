#include "TrayBadgePainter.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace Mattermost {

void TrayBadgePainter::paint(QPixmap& pixmap, TrayBadgeKind kind,
                             std::uint32_t attentionCount)
{
    if (pixmap.isNull() || kind == TrayBadgeKind::None) {
        return;
    }

    // Some platform icon engines return an opaque RGB pixmap. CompositionMode
    // Clear cannot cut transparency from that native pixel format, so ensure
    // a real alpha channel before applying the halo.
    if (!pixmap.hasAlphaChannel()) {
        const qreal originalDpr = pixmap.devicePixelRatioF();
        const QImage alphaImage = pixmap.toImage().convertToFormat(
            QImage::Format_ARGB32_Premultiplied);
        pixmap = QPixmap::fromImage(alphaImage, Qt::NoOpaqueDetection);
        pixmap.setDevicePixelRatio(originalDpr);
    }

    const qreal dpr = pixmap.devicePixelRatioF();
    const qreal width = pixmap.width() / dpr;
    const qreal height = pixmap.height() / dpr;
    const qreal scale = std::min(width, height) / 256.0;
    const qreal halo = std::max<qreal>(1.5, std::round(7.0 * scale));
    const qreal inset = halo;
    const bool attention = kind == TrayBadgeKind::Attention;

    // The blue notification is a dot, never a counter. The existing red
    // counter keeps the same visual size, but both badges occupy the same
    // bottom-right area and never appear together.
    const qreal badgeHeight = attention
        ? std::max<qreal>(9.0, std::round(80.0 * scale))
        : std::max<qreal>(6.0, std::round(68.0 * scale));
    qreal badgeWidth = badgeHeight;
    QString text;
    QFont font;
    if (attention) {
        text = attentionCount > 99
            ? QStringLiteral("99+") : QString::number(attentionCount);
        font = QApplication::font();
        font.setBold(true);
        font.setPixelSize(std::max(6, qRound(48.0 * scale)));
        const qreal padding = std::max<qreal>(1.0, std::round(10.0 * scale));
        badgeWidth = std::max(
            badgeHeight, QFontMetricsF(font).horizontalAdvance(text) + 2 * padding);
        badgeWidth = std::min(badgeWidth, width - 2 * inset);
        while (font.pixelSize() > 5
               && QFontMetricsF(font).horizontalAdvance(text) > badgeWidth - 2 * padding) {
            font.setPixelSize(font.pixelSize() - 1);
        }
    }

    const QRectF badgeRect(width - inset - badgeWidth,
                           height - inset - badgeHeight,
                           badgeWidth, badgeHeight);
    const QRectF haloRect = badgeRect.adjusted(-halo, -halo, halo, halo);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    // QSystemTrayIcon can sit against any panel/background color. Erase the
    // source icon's alpha in a thin pill/circular halo so no opaque outline
    // needs to be guessed from the desktop theme.
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(haloRect, haloRect.height() / 2.0,
                           haloRect.height() / 2.0);

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(attention ? QColor(220, 53, 69) : QColor(43, 126, 225));
    painter.drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);
    if (attention) {
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, text);
    }
}

} // namespace Mattermost
