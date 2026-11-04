/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "NativeChatRenderers.h"
#include "nativechat/themes/ThemeLayoutHelpers.h"

#include <QBoxLayout>
#include <QEvent>
#include <QFontMetricsF>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QTransform>
#include <QWidget>
#include <QtMath>

namespace {

using Canvas = NativeChatMessageCanvas;
using NativeChatThemeLayout::messageHeader;
using NativeChatThemeLayout::rowLayout;

// ---------------------------------------------------------------------------
// Theme descriptions: Modern, Classic and JSON slot trees.

const QHash<QString, Canvas::Part> &slotParts()
{
    static const QHash<QString, Canvas::Part> parts { { QStringLiteral("author"), Canvas::Author },
                                                      { QStringLiteral("timestamp"), Canvas::Timestamp },
                                                      { QStringLiteral("avatar"), Canvas::Avatar },
                                                      { QStringLiteral("body"), Canvas::Body },
                                                      { QStringLiteral("unreadSeparator"), Canvas::UnreadSeparator } };
    return parts;
}

bool validateLayout(const QJsonValue &node, QSet<QString> &seen, int depth = 0)
{
    if (depth > 8)
        return false;
    if (node.isString()) {
        const QString slot = node.toString();
        if (!slotParts().contains(slot) || seen.contains(slot))
            return false;
        seen.insert(slot);
        return true;
    }
    if (!node.isObject())
        return false;
    const QJsonObject group     = node.toObject();
    const QString     direction = group.value(QStringLiteral("direction")).toString();
    const QJsonArray  items     = group.value(QStringLiteral("items")).toArray();
    if ((direction != QLatin1String("horizontal") && direction != QLatin1String("vertical")) || items.isEmpty()
        || items.size() > 12)
        return false;
    for (const auto &child : items)
        if (!validateLayout(child, seen, depth + 1))
            return false;
    return true;
}

QBoxLayout *buildSlotLayout(const QJsonObject &node, const QHash<QString, QWidget *> &elements, int horizontalSpacing)
{
    const bool  horizontal = node.value(QStringLiteral("direction")).toString() == QLatin1String("horizontal");
    QBoxLayout *layout
        = horizontal ? static_cast<QBoxLayout *>(new QHBoxLayout) : static_cast<QBoxLayout *>(new QVBoxLayout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(horizontal ? horizontalSpacing : 3);
    for (const auto &item : node.value(QStringLiteral("items")).toArray()) {
        if (item.isObject()) {
            auto *nested = buildSlotLayout(item.toObject(), elements, horizontalSpacing);
            if (!horizontal && item.toObject().value(QStringLiteral("direction")) == QLatin1String("horizontal")) {
                // A horizontal header inside a vertical message column needs
                // its own height-for-width boundary. Qt6 otherwise distributes
                // available height inside the nested layout, leaving a large
                // blank gap between the header and the message document.
                auto *header = new QWidget;
                header->setObjectName(QStringLiteral("mamHeaderGroup"));
                header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
                header->setLayout(nested);
                layout->addWidget(header, 0, Qt::AlignTop);
            } else {
                layout->addLayout(nested, horizontal ? 1 : 0);
                if (!horizontal)
                    layout->setAlignment(nested, Qt::AlignTop);
            }
        } else if (auto *widget = elements.value(item.toString(), nullptr)) {
            const bool bodyOnLine = horizontal && item.toString() == QLatin1String("body");
            layout->addWidget(widget, bodyOnLine ? 1 : 0, Qt::AlignTop);
        }
    }
    return layout;
}

class DescriptionRenderer final : public NativeChatMessageRenderer {
public:
    explicit DescriptionRenderer(const NativeChatTheme &theme) :
        theme_(theme), modern_(theme.layout == NativeChatTheme::Modern)
    {
        if (!theme.layoutSpec.isEmpty()) {
            const auto    custom = QJsonDocument::fromJson(theme.layoutSpec.toUtf8());
            QSet<QString> seen;
            if (custom.isObject() && validateLayout(custom.object(), seen) && seen.contains(QStringLiteral("body"))) {
                tree_  = custom.object();
                slots_ = seen;
            }
        }
        // Classic without a tree of its own is the legacy text log: the time
        // and nickname are in the message text, so wrapped lines start at the
        // left edge instead of under the end of the nickname.
        textLog_ = tree_.isEmpty() && !modern_;
    }

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &info) const override
    {
        NativeChatPartOptions options;
        options.directionColors = true;
        // Modern's own arrangement always shows the avatar; a slot tree only
        // if it names it.
        const bool avatar  = !textLog_ && (tree_.isEmpty() || slots_.contains(QStringLiteral("avatar")));
        options.avatarSize = avatar ? theme_.avatarSize : 0;
        if (modern_) {
            options.boldAuthor     = true;
            options.timePointDelta = -1.0;
            options.timeAlpha      = 188; // Legible but unobtrusive.
        } else {
            // The legacy text log: "[HH:mm:ss] <nick> text" at full size.
            // Group chats painted the time in the same colour as the nickname.
            options.timeFormat      = QStringLiteral("[HH:mm:ss]");
            options.boldAuthor      = false;
            options.timeInNickColor = info.groupChat;
            if (textLog_) {
                options.header       = NativeChatPartOptions::HeaderInText;
                options.authorPrefix = QStringLiteral("<");
                options.authorSuffix = QStringLiteral(">");
            } else {
                options.authorSuffix = QStringLiteral(":");
            }
        }
        return options;
    }

    void arrange(Canvas &canvas, const NativeChatMessageInfo &info) const override
    {
        const int   vertical = modern_ ? 8 : 2;
        auto       *outer    = rowLayout(canvas, QMargins(12, vertical, 12, vertical), theme_.spacing);
        if (textLog_) {
            outer->addWidget(canvas.take(Canvas::Body), 1, Qt::AlignTop);
            return;
        }
        auto       *content  = new QWidget(canvas.row());
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        if (!tree_.isEmpty()) {
            // Classic and plugin descriptions choose any nested row/column
            // arrangement of the parts.
            outer->addWidget(content, 1, Qt::AlignTop);
            QHash<QString, QWidget *> elements;
            for (const auto &slot : slots_)
                if (auto *part = canvas.take(slotParts().value(slot)))
                    elements.insert(slot, part);
            content->setLayout(buildSlotLayout(tree_, elements, modern_ ? 10 : 4));
            return;
        }
        // Psi's original Modern geometry. Qt6 treats nested QBoxLayouts
        // differently from Qt5 when the body has a custom height-for-width
        // and the header is not a separate QWidget.
        if (auto *avatar = canvas.take(Canvas::Avatar))
            outer->addWidget(avatar, 0, Qt::AlignTop);
        outer->addWidget(content, 1, Qt::AlignTop);
        auto *column = new QVBoxLayout(content);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);
        if (info.kind == NativeChatMessageInfo::Message)
            column->addWidget(messageHeader(canvas, content), 0, Qt::AlignTop);
        column->addWidget(canvas.take(Canvas::Body), 0, Qt::AlignTop);
    }

private:
    NativeChatTheme theme_;
    bool            modern_;
    QJsonObject     tree_;
    QSet<QString>   slots_;
    bool            textLog_ = false;
};


}

namespace NativeChatRenderers {

std::unique_ptr<NativeChatMessageRenderer> forDescription(const NativeChatTheme &theme)
{
    return std::make_unique<DescriptionRenderer>(theme);
}

}
