/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../../plugins/include/nativechatthemeprovider.h"
#include "../../../plugins/include/nativechatrenderer.h"
#include "themes/ThemeLayoutHelpers.h"

#include <QBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QWidget>

namespace {
using Canvas = NativeChatMessageCanvas;
using NativeChatThemeLayout::messageHeader;
using NativeChatThemeLayout::rowLayout;

// The interpreter only knows the public semantic slots, not theme identities.
const QHash<QString, Canvas::Part> &slotParts()
{
    static const QHash<QString, Canvas::Part> parts { { QStringLiteral("author"), Canvas::Author },
                                                      { QStringLiteral("timestamp"), Canvas::Timestamp },
                                                      { QStringLiteral("avatar"), Canvas::Avatar },
                                                      { QStringLiteral("body"), Canvas::Body },
                                                      { QStringLiteral("unreadSeparator"), Canvas::UnreadSeparator },
                                                      { QStringLiteral("dateSeparator"), Canvas::DateSeparator },
                                                      { QStringLiteral("noticeTimestamp"), Canvas::NoticeTimestamp } };
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
    explicit DescriptionRenderer(const NativeChatTheme &theme) : theme_(theme)
    {
        if (theme_.layoutSpec.isEmpty())
            return;
        const auto custom = QJsonDocument::fromJson(theme_.layoutSpec.toUtf8());
        QSet<QString> seen;
        if (custom.isObject() && validateLayout(custom.object(), seen)
            && seen.contains(QStringLiteral("body"))) {
            tree_ = custom.object();
            slots_ = seen;
            layoutTimeFormat_ = tree_.value(QStringLiteral("timestampFormat")).toString();
        }
    }

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override
    {
        NativeChatPartOptions options;
        options.directionColors = true;
        options.avatarSize = (tree_.isEmpty() || slots_.contains(QStringLiteral("avatar")))
                                 ? theme_.avatarSize : 0;
        options.boldAuthor = true;
        options.timePointDelta = -1.0;
        options.timeAlpha = 188;
        options.noticeHoverTime = slots_.contains(QStringLiteral("noticeTimestamp"));
        options.timeFormat = theme_.timestampFormat.isEmpty() ? layoutTimeFormat_ : theme_.timestampFormat;
        return options;
    }

    void arrange(Canvas &canvas, const NativeChatMessageInfo &info) const override
    {
        auto *outer = rowLayout(canvas, QMargins(12, 8, 12, 8), theme_.spacing);
        auto *content = new QWidget(canvas.row());
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        if (tree_.isEmpty()) {
            // Standard fallback for a description without a valid slot tree.
            // It is not an alias or a special case for any built-in theme.
            if (auto *avatar = canvas.take(Canvas::Avatar))
                outer->addWidget(avatar, 0, Qt::AlignTop);
            outer->addWidget(content, 1, Qt::AlignTop);
            auto *column = new QVBoxLayout(content);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(3);
            if (info.kind == NativeChatMessageInfo::Message)
                column->addWidget(messageHeader(canvas, content), 0, Qt::AlignTop);
            column->addWidget(canvas.take(Canvas::Body), 0, Qt::AlignTop);
            return;
        }

        outer->addWidget(content, 1, Qt::AlignTop);
        QHash<QString, QWidget *> elements;
        for (const auto &slot : slots_)
            if (auto *part = canvas.take(slotParts().value(slot)))
                elements.insert(slot, part);
        content->setLayout(buildSlotLayout(tree_, elements, theme_.spacing));
    }

private:
    NativeChatTheme theme_;
    QJsonObject tree_;
    QSet<QString> slots_;
    QString layoutTimeFormat_;
};
}

std::shared_ptr<const NativeChatMessageRenderer> NativeChatTheme::renderer() const
{
    if (renderer_)
        return renderer_;
    return std::make_shared<DescriptionRenderer>(*this);
}
