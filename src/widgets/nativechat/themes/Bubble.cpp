/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include "ThemeLayoutHelpers.h"
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QLabel>
#include <QPaintEvent>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QtMath>
#include <memory>
namespace {
using Canvas = NativeChatMessageCanvas;
using NativeChatThemeLayout::messageHeader;
using NativeChatThemeLayout::rowLayout;
QColor mix(const QColor &from, const QColor &to, qreal amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}
}
namespace NativeChatBuiltins {
constexpr int BubbleMaxTextColumns = 60;
}
namespace NativeChatBuiltins {

// A rounded frame, for messages with a tail at its top pointing sideways at
// the avatar. Style sheets select it as #mamBubble by its tone and tailSide
// properties and can set its colours, corner radius and text width.
class BubbleFrame final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(int radius READ radius WRITE setRadius)
    Q_PROPERTY(int textColumns READ textColumns WRITE setTextColumns)

public:
    enum Side { Left, Right, NoTail };
    enum Tone { Incoming, Own, Notice, Alert };
    static constexpr int TailWidth  = 9;
    static constexpr int TailTop    = 11; // From the top of the bubble to where the tail leaves it.
    static constexpr int TailHeight = 16; // Along the bubble's edge.

    BubbleFrame(Side tail, Tone tone, QWidget *parent) : QWidget(parent), tail_(tail), tone_(tone)
    {
        setObjectName(QStringLiteral("mamBubble"));
        setProperty("tailSide", tail == Left    ? QStringLiteral("left")
                                : tail == Right ? QStringLiteral("right")
                                                : QStringLiteral("none"));
        setProperty("tone", tone == Own      ? QStringLiteral("own")
                            : tone == Notice ? QStringLiteral("notice")
                            : tone == Alert  ? QStringLiteral("alert")
                                             : QStringLiteral("incoming"));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto *column = new QVBoxLayout(this);
        if (tone == Notice)
            column->setContentsMargins(14, 5, 14, 6);
        else
            column->setContentsMargins(12 + (tail == Left ? TailWidth : 0), 6, 12 + (tail == Right ? TailWidth : 0),
                                       7);
        column->setSpacing(3);
        updateMaximumWidth();
    }

    QVBoxLayout *column() const { return static_cast<QVBoxLayout *>(layout()); }

    // Invalid colours follow the palette.
    QColor fillColor() const { return fillColor_; }
    void   setFillColor(const QColor &color)
    {
        fillColor_ = color;
        update();
    }
    QColor borderColor() const { return borderColor_; }
    void   setBorderColor(const QColor &color)
    {
        borderColor_ = color;
        update();
    }
    int  radius() const { return radius_; }
    void setRadius(int radius)
    {
        radius_ = qBound(0, radius, 30);
        update();
    }
    int  textColumns() const { return textColumns_; }
    void setTextColumns(int columns)
    {
        textColumns_ = qBound(10, columns, 1000);
        updateMaximumWidth();
    }

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
        const QColor warning = surface.lightness() > 128 ? QColor(185, 35, 45) : QColor(255, 120, 125);
        const QColor fill    = fillColor_.isValid() ? fillColor_
               : tone_ == Own                       ? mix(surface, colors.color(QPalette::Highlight), 0.18)
               : tone_ == Notice                    ? mix(surface, text, 0.035)
               : tone_ == Alert                     ? mix(surface, warning, 0.13)
                                                    : mix(surface, text, 0.07);
        const QColor border  = borderColor_.isValid()
             ? borderColor_
             : tone_ == Alert ? mix(surface, warning, 0.75)
                              : mix(surface, text, tone_ == Own ? 0.20 : tone_ == Notice ? 0.12 : 0.16);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(border.alpha() ? QPen(border, 1.0) : QPen(Qt::NoPen));
        painter.setBrush(fill);
        painter.drawPath(outline());
    }

private:
    // Drawn for a tail on the left, then mirrored for the right.
    QPainterPath outline() const
    {
        const qreal w = width(), h = height();
        if (tail_ == NoTail) {
            QPainterPath shape;
            const qreal  radius = qMin<qreal>(radius_ + 2, (h - 1.0) / 2.0);
            shape.addRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), radius, radius);
            return shape;
        }
        const QRectF box(TailWidth + 0.5, 0.5, w - TailWidth - 1.0, h - 1.0);
        QPainterPath shape;
        shape.addRoundedRect(box, radius_, radius_);

        // A hooked tail: a nearly level upper edge out to the tip, which sits
        // beside the upper part of the avatar, and a concave lower edge back.
        const qreal  edge  = box.left();
        const qreal  top   = box.top() + TailTop;
        const qreal  tipY  = top + 3.0;
        const qreal  inset = qMax<qreal>(radius_, 2.0) / 2.0;
        QPainterPath tail;
        tail.moveTo(edge + inset, top);
        tail.lineTo(edge, top);
        tail.quadTo(edge - TailWidth * 0.45, top + 0.5, 0.5, tipY);
        tail.quadTo(edge - 1.0, tipY + 2.0, edge, top + TailHeight);
        tail.lineTo(edge + inset, top + TailHeight);
        tail.closeSubpath();
        shape = shape.united(tail).simplified();

        if (tail_ == Right)
            shape = QTransform(-1, 0, 0, 1, w, 0).map(shape);
        return shape;
    }

    void updateMaximumWidth()
    {
        const QMargins margins = layout() ? layout()->contentsMargins() : QMargins();
        setMaximumWidth(qCeil(QFontMetricsF(font()).averageCharWidth() * textColumns_) + margins.left()
                        + margins.right());
    }

    Side   tail_;
    Tone   tone_;
    QColor fillColor_;
    QColor borderColor_;
    int    radius_      = 10;
    int    textColumns_ = BubbleMaxTextColumns;
};

}


namespace {

using NativeChatBuiltins::BubbleFrame;

// Bubble places the date in its own centred, tailless bubble before the
// message, and the unread boundary below it. Neither becomes a list row.
QHBoxLayout *bubbleRowLayout(Canvas &canvas)
{
    auto *date = canvas.take(Canvas::DateSeparator);
    auto *separator = canvas.take(Canvas::UnreadSeparator);
    if (!date && !separator)
        return rowLayout(canvas, QMargins(12, 4, 12, 4), 6);

    auto *column = new QVBoxLayout(canvas.row());
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    if (date) {
        auto *dateRow = new QWidget(canvas.row());
        dateRow->setObjectName(QStringLiteral("mamBubbleDateRow"));
        auto *dateLayout = new QHBoxLayout(dateRow);
        dateLayout->setContentsMargins(12, 5, 12, 4);
        auto *dateBubble = new BubbleFrame(BubbleFrame::NoTail, BubbleFrame::Notice, dateRow);
        // Keep the date bubble independently selectable from message bubbles
        // by QSS and findChild() consumers.
        dateBubble->setObjectName(QStringLiteral("mamDateBubble"));
        dateBubble->setProperty("dateSeparator", true);
        dateBubble->column()->addWidget(date);
        dateLayout->addStretch(1);
        dateLayout->addWidget(dateBubble, 0, Qt::AlignCenter);
        dateLayout->addStretch(1);
        column->addWidget(dateRow, 0, Qt::AlignTop);
    }

    auto *message = new QWidget(canvas.row());
    message->setObjectName(QStringLiteral("mamBubbleMessage"));
    message->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    column->addWidget(message, 0, Qt::AlignTop);

    auto *outer = new QHBoxLayout(message);
    outer->setContentsMargins(12, 4, 12, 4);
    outer->setSpacing(6);
    if (auto *selection = canvas.take(Canvas::Selection))
        outer->addWidget(selection, 0, Qt::AlignTop);

    if (separator) {
        auto *divider = new QWidget(canvas.row());
        divider->setObjectName(QStringLiteral("mamBubbleUnreadDivider"));
        auto *dividerLayout = new QHBoxLayout(divider);
        dividerLayout->setContentsMargins(12, 2, 12, 2);
        dividerLayout->addWidget(separator);
        column->addWidget(divider, 0, Qt::AlignTop);
    }
    return outer;
}

// The clock is an absolute-positioned sibling of the bubble. It must not
// participate in the layout, or a hidden clock shifts centred notice bubbles.
class NoticeClockPositioner final : public QObject {
public:
    NoticeClockPositioner(QLabel *clock, QWidget *bubble, QWidget *row)
        : QObject(clock), clock_(clock), bubble_(bubble), row_(row)
    {
        clock_->setParent(row_);
        bubble_->installEventFilter(this);
        row_->installEventFilter(this);
        clock_->show();
        reposition();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == bubble_ || watched == row_) {
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::LayoutRequest:
            case QEvent::LayoutDirectionChange:
            case QEvent::ParentChange:
                reposition();
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void reposition()
    {
        if (!clock_ || !bubble_ || !row_)
            return;
        clock_->adjustSize();
        const QPoint origin = bubble_->mapTo(row_, QPoint());
        const bool rtl = bubble_->layoutDirection() == Qt::RightToLeft;
        constexpr int gap = 6;
        const int x = rtl ? origin.x() + bubble_->width() + gap
                          : origin.x() - clock_->width() - gap;
        clock_->move(x, origin.y() + (bubble_->height() - clock_->height()) / 2);
        clock_->raise();
    }

    QPointer<QLabel>  clock_;
    QPointer<QWidget> bubble_;
    QPointer<QWidget> row_;
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
        options.noticeHoverTime = true;
        return options;
    }

    void arrange(Canvas &canvas, const NativeChatMessageInfo &info) const override
    {
        if (info.kind != NativeChatMessageInfo::Message) {
            // Notices and the subject: a centred bubble without a tail.
            auto *outer  = bubbleRowLayout(canvas);
            auto *bubble = new BubbleFrame(BubbleFrame::NoTail, BubbleFrame::Notice, canvas.row());
            bubble->column()->addWidget(canvas.take(Canvas::Body), 0, Qt::AlignTop);
            outer->addStretch(1);
            outer->addWidget(bubble, 0, Qt::AlignTop);
            outer->addStretch(1);
            if (auto *clock = qobject_cast<QLabel *>(canvas.take(Canvas::NoticeTimestamp)))
                new NoticeClockPositioner(clock, bubble, canvas.row());
            canvas.setActionAnchor(bubble);
            return;
        }
        auto *outer  = bubbleRowLayout(canvas);
        auto *avatar = canvas.take(Canvas::Avatar);
        auto *bubble = new BubbleFrame(info.local ? BubbleFrame::Right : BubbleFrame::Left,
                                       info.alert ? BubbleFrame::Alert
                                                  : info.local ? BubbleFrame::Own : BubbleFrame::Incoming,
                                       canvas.row());
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

namespace NativeChatBuiltins {
NativeChatTheme bubble()
{
    NativeChatTheme theme;
    theme.id = QStringLiteral("bubble");
    theme.name = QCoreApplication::translate("NativeChatThemes", "Bubble");
    theme.spacing = 6;
    theme.hoverHighlight = false;
    theme.description = QCoreApplication::translate("NativeChatThemes", "Speech bubbles, your messages on the right");
    theme.setRenderer(std::make_shared<BubbleRenderer>());
    return theme;
}
}
#include "Bubble.moc"
