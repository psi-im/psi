/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../../../../plugins/include/nativechatrenderer.h"
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

// Shared layout primitives; each theme owns its own message geometry.
namespace NativeChatThemeLayout {
using Canvas = NativeChatMessageCanvas;

inline QHBoxLayout *rowLayout(Canvas &canvas, const QMargins &margins, int spacing)
{
    auto *outer = new QHBoxLayout(canvas.row());
    outer->setContentsMargins(margins);
    outer->setSpacing(spacing);
    if (auto *selection = canvas.take(Canvas::Selection))
        outer->addWidget(selection, 0, Qt::AlignTop);
    return outer;
}

inline QWidget *messageHeader(Canvas &canvas, QWidget *parent)
{
    auto *header = new QWidget(parent);
    header->setObjectName(QStringLiteral("mamHeader"));
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    if (auto *author = canvas.take(Canvas::Author))
        layout->addWidget(author);
    if (auto *time = canvas.take(Canvas::Timestamp))
        layout->addWidget(time);
    layout->addStretch();
    return header;
}
}
