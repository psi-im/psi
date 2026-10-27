/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "NativeChatRenderers.h"

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

// ---------------------------------------------------------------------------
// Theme descriptions: Modern, Classic and JSON slot trees.

const QHash<QString, Canvas::Part> &slotParts()
{
    static const QHash<QString, Canvas::Part> parts { { QStringLiteral("author"), Canvas::Author },
                                                      { QStringLiteral("timestamp"), Canvas::Timestamp },
                                                      { QStringLiteral("avatar"), Canvas::Avatar },
                                                      { QStringLiteral("body"), Canvas::Body } };
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

QHBoxLayout *rowLayout(Canvas &canvas, const QMargins &margins, int spacing)
{
    auto *outer = new QHBoxLayout(canvas.row());
    outer->setContentsMargins(margins);
    outer->setSpacing(spacing);
    if (auto *selection = canvas.take(Canvas::Selection))
        outer->addWidget(selection, 0, Qt::AlignTop);
    return outer;
}

// Nickname and time on one line, as in Modern.
QWidget *messageHeader(Canvas &canvas, QWidget *parent)
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
        if (tree_.isEmpty() && !modern_) {
            tree_ = QJsonDocument::fromJson(R"({"direction":"horizontal","items":["timestamp","author","body"]})")
                        .object();
            slots_ = { QStringLiteral("timestamp"), QStringLiteral("author"), QStringLiteral("body") };
        }
    }

    NativeChatPartOptions partOptions(const NativeChatMessageInfo &info) const override
    {
        NativeChatPartOptions options;
        // Modern's own arrangement always shows the avatar; a slot tree only
        // if it names it.
        const bool avatar  = tree_.isEmpty() || slots_.contains(QStringLiteral("avatar"));
        options.avatarSize = avatar ? theme_.avatarSize : 0;
        if (modern_) {
            options.boldAuthor     = true;
            options.timePointDelta = -1.0;
            options.timeAlpha      = 188; // Legible but unobtrusive.
        } else {
            // The legacy text log: full-size [HH:mm:ss] and "nick:". Group
            // chats painted the time in the same colour as the nickname.
            options.timeFormat      = QStringLiteral("[HH:mm:ss]");
            options.authorSuffix    = QStringLiteral(":");
            options.boldAuthor      = false;
            options.timeInNickColor = info.groupChat;
        }
        return options;
    }

    void arrange(Canvas &canvas, const NativeChatMessageInfo &info) const override
    {
        const int   vertical = modern_ ? 8 : 2;
        auto       *outer    = rowLayout(canvas, QMargins(12, vertical, 12, vertical), theme_.spacing);
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
};

// ---------------------------------------------------------------------------
// Speech bubbles.

QColor mix(const QColor &from, const QColor &to, qreal amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

// A rounded frame with a tail at its top, pointing sideways at the avatar.
class BubbleFrame final : public QWidget {
public:
    enum Side { Left, Right };
    static constexpr int TailWidth  = 9;
    static constexpr int TailTop    = 11; // From the top of the bubble to where the tail leaves it.
    static constexpr int TailHeight = 16; // Along the bubble's edge.
    static constexpr int Radius     = 10;

    BubbleFrame(Side tail, bool local, QWidget *parent) : QWidget(parent), tail_(tail), local_(local)
    {
        setObjectName(QStringLiteral("mamBubble"));
        setProperty("tailSide", tail == Left ? QStringLiteral("left") : QStringLiteral("right"));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto *column = new QVBoxLayout(this);
        column->setContentsMargins(12 + (tail == Left ? TailWidth : 0), 6, 12 + (tail == Right ? TailWidth : 0), 7);
        column->setSpacing(3);
        updateMaximumWidth();
    }

    QVBoxLayout *column() const { return static_cast<QVBoxLayout *>(layout()); }

protected:
    void changeEvent(QEvent *event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange)
            updateMaximumWidth();
    }

    void paintEvent(QPaintEvent *) override
    {
        // Follow the row's surface, which Psi tints on hover and selection.
        // A chat window style sheet can stop palette propagation to nested
        // widgets, so read the row itself.
        QPalette colors = palette();
        for (QWidget *ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (ancestor->objectName() == QLatin1String("mamMessageRow")) {
                colors = ancestor->palette();
                break;
            }
        }
        const QColor surface = colors.color(QPalette::Base);
        const QColor text    = colors.color(QPalette::Text);
        const QColor fill    = local_ ? mix(surface, colors.color(QPalette::Highlight), 0.18) : mix(surface, text, 0.07);
        const QColor border  = mix(surface, text, local_ ? 0.20 : 0.16);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(border, 1.0));
        painter.setBrush(fill);
        painter.drawPath(outline());
    }

private:
    // Drawn for a tail on the left, then mirrored for the right.
    QPainterPath outline() const
    {
        const qreal  w = width(), h = height();
        const QRectF box(TailWidth + 0.5, 0.5, w - TailWidth - 1.0, h - 1.0);
        QPainterPath shape;
        shape.addRoundedRect(box, Radius, Radius);

        // A hooked tail: a nearly level upper edge out to the tip, which sits
        // beside the upper part of the avatar, and a concave lower edge back.
        const qreal  edge = box.left();
        const qreal  top  = box.top() + TailTop;
        const qreal  tipY = top + 3.0;
        QPainterPath tail;
        tail.moveTo(edge + Radius / 2.0, top);
        tail.lineTo(edge, top);
        tail.quadTo(edge - TailWidth * 0.45, top + 0.5, 0.5, tipY);
        tail.quadTo(edge - 1.0, tipY + 2.0, edge, top + TailHeight);
        tail.lineTo(edge + Radius / 2.0, top + TailHeight);
        tail.closeSubpath();
        shape = shape.united(tail).simplified();

        if (tail_ == Right)
            shape = QTransform(-1, 0, 0, 1, w, 0).map(shape);
        return shape;
    }

    void updateMaximumWidth()
    {
        const QMargins margins = layout() ? layout()->contentsMargins() : QMargins();
        setMaximumWidth(qCeil(QFontMetricsF(font()).averageCharWidth() * NativeChatRenderers::BubbleMaxTextColumns)
                        + margins.left() + margins.right());
    }

    Side tail_;
    bool local_;
};

// Messages in speech bubbles, based on Modern. Incoming messages sit on the
// left and our own on the right, each with the avatar on its outer side and
// the bubble's tail pointing at it.
class BubbleRenderer final : public NativeChatMessageRenderer {
public:
    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override
    {
        NativeChatPartOptions options;
        options.avatarSize     = 40;
        options.boldAuthor     = true;
        options.timePointDelta = -1.0;
        options.timeAlpha      = 188;
        return options;
    }

    void arrange(Canvas &canvas, const NativeChatMessageInfo &info) const override
    {
        if (info.kind != NativeChatMessageInfo::Message) {
            // Notices and the subject stay plain, centred lines between bubbles.
            auto *outer = rowLayout(canvas, QMargins(12, 4, 12, 4), 8);
            outer->addWidget(canvas.take(Canvas::Body), 1, Qt::AlignTop);
            return;
        }
        auto *outer  = rowLayout(canvas, QMargins(12, 4, 12, 4), 6);
        auto *avatar = canvas.take(Canvas::Avatar);
        auto *bubble = new BubbleFrame(info.local ? BubbleFrame::Right : BubbleFrame::Left, info.local, canvas.row());
        bubble->column()->addWidget(messageHeader(canvas, bubble), 0, Qt::AlignTop);
        bubble->column()->addWidget(canvas.take(Canvas::Body), 0, Qt::AlignTop);
        if (info.local) {
            outer->addStretch(1);
            outer->addWidget(bubble, 0, Qt::AlignTop);
            if (avatar)
                outer->addWidget(avatar, 0, Qt::AlignTop);
        } else {
            if (avatar)
                outer->addWidget(avatar, 0, Qt::AlignTop);
            outer->addWidget(bubble, 0, Qt::AlignTop);
            outer->addStretch(1);
        }
        canvas.setActionAnchor(bubble);
    }
};

}

namespace NativeChatRenderers {

std::unique_ptr<NativeChatMessageRenderer> forDescription(const NativeChatTheme &theme)
{
    return std::make_unique<DescriptionRenderer>(theme);
}

const NativeChatMessageRenderer *bubble()
{
    static const BubbleRenderer renderer;
    return &renderer;
}

}
