/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CachedTextImageHandler.h"
#include "PixmapLruCache.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QUrl>
#include <QtMath>

namespace {
QImage resourceImage(QTextDocument *document, const QTextImageFormat &format)
{
    const QVariant resource = document->resource(QTextDocument::ImageResource, QUrl(format.name()));
    if (resource.userType() == QMetaType::QImage)
        return qvariant_cast<QImage>(resource);
    return {};
}

qreal dpiScale(QTextDocument *document)
{
    auto *layout = document->documentLayout();
    auto *device = layout ? layout->paintDevice() : nullptr;
    if (!device)
        return 1.0;
    auto *screen = QGuiApplication::primaryScreen();
    const qreal defaultDpi = screen ? screen->logicalDotsPerInchY() : 96.0;
    return defaultDpi > 0 ? device->logicalDpiY() / defaultDpi : 1.0;
}
}

CachedTextImageHandler::CachedTextImageHandler(QAbstractTextDocumentLayout *layout, QObject *parent)
    : QObject(parent)
{
    // Preserve Qt's built-in handler for non-QImage resources (file://, qrc,
    // QByteArray and QPixmap). It belongs to the layout and is never deleted
    // by registerHandler when the registration is replaced.
    fallback_ = layout->handlerForObject(QTextFormat::ImageObject);
    layout->registerHandler(QTextFormat::ImageObject, this);
}

QSizeF CachedTextImageHandler::intrinsicSize(QTextDocument *document, int position,
                                             const QTextFormat &format)
{
    const QTextImageFormat imageFormat = format.toImageFormat();
    const QImage image = resourceImage(document, imageFormat);
    if (image.isNull())
        return fallback_ ? fallback_->intrinsicSize(document, position, format) : QSizeF();

    // Match QTextImageHandler's dimension/aspect handling without asking for
    // a QPixmap, which forces an expensive image conversion during relayout.
    const bool hasWidth = imageFormat.hasProperty(QTextFormat::ImageWidth);
    const bool hasHeight = imageFormat.hasProperty(QTextFormat::ImageHeight);
    const QSizeF sourceSize = QSizeF(image.size()) / qMax<qreal>(1.0, image.devicePixelRatio());
    qreal width = hasWidth ? imageFormat.width() : sourceSize.width();
    qreal height = hasHeight ? imageFormat.height() : sourceSize.height();
    if (hasWidth && !hasHeight && sourceSize.width() > 0)
        height = width * sourceSize.height() / sourceSize.width();
    if (!hasWidth && hasHeight && sourceSize.height() > 0)
        width = height * sourceSize.width() / sourceSize.height();
    return QSizeF(qRound(width), qRound(height)) * dpiScale(document);
}

void CachedTextImageHandler::drawObject(QPainter *painter, const QRectF &rect,
                                       QTextDocument *document, int position,
                                       const QTextFormat &format)
{
    const QTextImageFormat imageFormat = format.toImageFormat();
    const QImage image = resourceImage(document, imageFormat);
    if (image.isNull()) {
        if (fallback_)
            fallback_->drawObject(painter, rect, document, position, format);
        return;
    }

    // Stable logical identifier + QImage.cacheKey() (content revision).
    // The temporary cache is the only additional owner of the pixmap.
    const QPixmap pixmap = Psi::PixmapLruCache::instance().fromImage(imageFormat.name(), image);
    if (!pixmap.isNull())
        painter->drawPixmap(rect, pixmap, QRectF(pixmap.rect()));
}
