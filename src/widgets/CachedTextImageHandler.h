/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QAbstractTextDocumentLayout>
#include <QObject>
#include <QTextObjectInterface>

// QTextDocument owns QImages as resources. Qt's default image handler converts
// each QImage to QPixmap even while merely measuring intrinsic dimensions.
// Cache conversions temporarily without storing pixmaps in QTextDocument.
class CachedTextImageHandler final : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit CachedTextImageHandler(QAbstractTextDocumentLayout *layout, QObject *parent = nullptr);

    QSizeF intrinsicSize(QTextDocument *document, int position, const QTextFormat &format) override;
    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *document,
                    int position, const QTextFormat &format) override;

private:
    QTextObjectInterface *fallback_ = nullptr; // Owned by QTextDocumentLayout.
};
