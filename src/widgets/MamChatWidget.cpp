/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamChatWidget.h"
#include "ChatMessageText.h"
#include "MamMessageIdentity.h"
#include "iconset.h"
#include "psirichtext.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFontInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QWheelEvent>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {

// Base for the marks beside a timestamp (delivery checks, edit pencil). They
// share one height and sit on the timestamp's text baseline.
class TimestampMark : public QLabel {
public:
    TimestampMark(QLabel *timestamp, QWidget *parent) : QLabel(parent), timestamp_(timestamp) { }

    void setTimestampFont(const QFont &timeFont)
    {
        setFont(timeFont);
        fontChanged(QFontMetricsF(timeFont));
        update();
    }

protected:
    // Timestamps are either digits ("22:43") or mostly lowercase ("34 minutes
    // ago"). A mark as tall as the digits looks raised beside lowercase text,
    // one as tall as the x-height looks small beside digits; use the middle.
    static qreal markHeight(const QFontMetricsF &metrics)
    {
        return qMax<qreal>(6.0, (metrics.xHeight() + metrics.capHeight()) / 2.0);
    }

    // The timestamp's baseline in this widget's coordinates. QLabel centres
    // its line vertically in its contents rect using fractional metrics, and
    // the widgets may differ in height inside the layout. A baseline derived
    // from this widget's own box drifted by a pixel for point-sized fonts, so
    // take it from the label itself.
    qreal textBaseline() const
    {
        const QLabel       *label   = timestamp_ ? timestamp_.data() : this;
        const QFontMetricsF metrics(label->font());
        const QRect         box     = label->contentsRect();
        const qreal         lineTop = box.top() + (box.height() - (metrics.ascent() + metrics.descent())) / 2.0;
        const qreal         offset
            = label == this ? 0 : label->mapTo(window(), QPoint()).y() - mapTo(window(), QPoint()).y();
        return qRound(lineTop + metrics.ascent()) + offset;
    }

    virtual void fontChanged(const QFontMetricsF &metrics) = 0;

private:
    QPointer<QLabel> timestamp_;
};

// Compact, overlapping checks (like a messenger read indicator), never two
// font glyphs. Using a painter keeps the shape identical under Qt5/Qt6,
// regardless of the platform font, palette, or device pixel ratio.
class DeliveryIndicator final : public TimestampMark {
public:
    DeliveryIndicator(bool read, QLabel *timestamp, QWidget *parent) : TimestampMark(timestamp, parent), read_(read)
    {
        setObjectName(QStringLiteral("mamDeliveryIndicator"));
        setProperty("deliveryState", read ? QStringLiteral("read") : QStringLiteral("delivered"));
        setForegroundRole(QPalette::WindowText);
        setTimestampFont(font());
    }

protected:
    void fontChanged(const QFontMetricsF &metrics) override
    {
        penWidth_ = qMax<qreal>(1.2, metrics.xHeight() * 0.11);
        // Round caps and joins extend half a pen width beyond the stroke
        // centre line, so the centre lines span the mark height minus one pen
        // width across the 7 design units between the stroke tips (y=2) and
        // the vertex (y=9).
        scale_ = qMax<qreal>(0.5, (markHeight(metrics) - penWidth_) / 7.0);
        // Match the timestamp label's line box so both share one baseline.
        setFixedSize(qCeil((designRight() - designLeft) * scale_ + penWidth_), QFontMetrics(font()).height());
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal baseline = textBaseline();
        const qreal half     = penWidth_ / 2.0;
        painter.translate(half - designLeft * scale_, baseline - half - designVertex * scale_);
        painter.scale(scale_, scale_);

        QColor color = palette().color(QPalette::Text);
        color.setAlpha(155); // Keep each narrow diagonal distinct at small sizes.
        QPen pen(color, penWidth_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        pen.setCosmetic(true); // Device pixels, so the edge math above holds at any scale.
        painter.setPen(pen);

        // Keep the long rising strokes separated. The former geometry put
        // the two check vertices only 2.4 design units apart, which collapsed
        // to a blurred line at the small native-chat timestamp size.
        QPainterPath first;
        first.moveTo(designLeft, 6.0);
        first.lineTo(4.0, designVertex);
        first.lineTo(10.7, designTip);
        painter.drawPath(first);

        if (read_) {
            // Only the short exposed portion of the second downstroke is
            // drawn. Its hidden start would fall behind the first check.
            // The second vertex and rising stroke are 4.5 units to the
            // right, making both strokes independently visible when scaled.
            QPainterPath second;
            second.moveTo(7.4, 7.9);
            second.lineTo(8.5, designVertex);
            second.lineTo(15.2, designTip);
            painter.drawPath(second);
        }
    }

private:
    static constexpr qreal designLeft   = 1.0; // leftmost stroke point
    static constexpr qreal designTip    = 2.0; // y of the rising stroke tips
    static constexpr qreal designVertex = 9.0; // y of the check vertices
    qreal                  designRight() const { return read_ ? 15.2 : 10.7; }

    bool  read_;
    qreal penWidth_ = 1.2;
    qreal scale_    = 1.0;
};

// A Psi iconset icon (edit pencil, encryption lock) at the timestamp font
// size, centred on the delivery checks. These icons are thin shapes filling
// their 16 px box, so shrinking them to the check height made them nearly
// invisible.
class IconMark final : public TimestampMark {
public:
    IconMark(const QString &iconName, QLabel *timestamp, QWidget *parent) :
        TimestampMark(timestamp, parent), iconName_(iconName)
    {
        setTimestampFont(font());
    }

protected:
    void fontChanged(const QFontMetricsF &metrics) override
    {
        const QString &name = iconName_;
        const PsiIcon *icon = IconsetFactory::iconPtr(name);
        const int            native = icon && icon->size().height() > 0 ? icon->size().height() : 16;
        const int            target = qMax(8, QFontInfo(font()).pixelSize());
        // Within a pixel of the icon's own size, draw it unscaled and crisp.
        size_       = qAbs(target - native) <= 1 ? native : target;
        markCenter_ = markHeight(metrics) / 2.0;
        setFixedSize(size_, qMax(size_, QFontMetrics(font()).height()));

        const qreal ratio  = size_ == native ? 1.0 : devicePixelRatioF();
        QPixmap     pixmap = IconsetFactory::iconPixmap(name, QSize(qRound(size_ * ratio), qRound(size_ * ratio)));
        if (!pixmap.isNull() && pixmap.height() != qRound(size_ * ratio))
            pixmap = pixmap.scaled(qRound(size_ * ratio), qRound(size_ * ratio), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(ratio);
        setPixmap(pixmap); // Keeps pixmap() meaningful for tests and tools.
    }

    void paintEvent(QPaintEvent *) override
    {
        const QPixmap icon = pixmap(Qt::ReturnByValue);
        if (icon.isNull())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        // Same vertical centre as the checks beside it; whole pixels keep an
        // unscaled icon sharp.
        const int top = qRound(textBaseline() - markCenter_ - size_ / 2.0);
        painter.drawPixmap(QRect(0, top, size_, size_), icon);
    }

private:
    QString iconName_;
    int     size_       = 16;
    qreal   markCenter_ = 4.0;
};

} // namespace

MamChatWidget::MamChatWidget(QWidget *parent) : Mattermost::LongListWidget(parent)
{
    setProperty("psiMamOverlay", true);
    theme_.layout = NativeChatTheme::Modern;
    // Coalesced single-shot timer: no updates while a chat is hidden and
    // no per-second activity. Only the next visible relative-time boundary
    // schedules a wake-up.
    timestampTimer_ = new QTimer(this);
    timestampTimer_->setObjectName(QStringLiteral("mamTimestampRefresh"));
    timestampTimer_->setSingleShot(true);
    timestampTimer_->setTimerType(Qt::CoarseTimer);
    connect(timestampTimer_, &QTimer::timeout, this, &MamChatWidget::updateTimestamps);
    // Deliberately visible in normal terminal output. The reported XMPP
    // software_version may be stale after a CMake reconfigure/rebuild.
    qInfo().noquote() << "[mam-history] paging-guard-v3 activated; widget=" << this;
    setDefaultItemHeight(72);
    setMaterializationLimit(160);
    setRequestBlockSize(40);
    setHoverHighlightEnabled(false);

    // MatterLeast-style floating whole-message selection controls.
    selectionToolbar_ = new QFrame(viewport());
    selectionToolbar_->setObjectName(QStringLiteral("mamSelectionToolbar"));
    selectionToolbar_->setFrameShape(QFrame::StyledPanel);
    selectionToolbar_->setAutoFillBackground(true);
    auto *tools = new QHBoxLayout(selectionToolbar_);
    tools->setContentsMargins(8, 4, 8, 4);
    tools->setSpacing(6);
    selectionCountLabel_ = new QLabel(selectionToolbar_);
    tools->addWidget(selectionCountLabel_);
    auto *copy = new QPushButton(tr("Copy"), selectionToolbar_);
    copy->setObjectName(QStringLiteral("mamCopySelected"));
    auto *cancel = new QPushButton(tr("Cancel"), selectionToolbar_);
    tools->addWidget(copy);
    tools->addWidget(cancel);
    connect(copy, &QPushButton::clicked, this, &MamChatWidget::copySelectedMessages);
    connect(cancel, &QPushButton::clicked, this, &MamChatWidget::cancelMessageSelection);
    selectionToolbar_->hide();
    // MatterLeast-style floating message actions: one rounded overlay frame
    // with [quick reactions | react | more]. It is anchored at its right edge,
    // so the quick-reaction slot grows leftwards and the react/more buttons
    // never move under the pointer. Rarely used and destructive actions stay
    // behind the "more" menu.
    messageActions_ = new QFrame(viewport());
    messageActions_->setObjectName(QStringLiteral("mamMessageActions"));
    messageActions_->setFrameShape(QFrame::NoFrame);
    messageActions_->setStyleSheet(QStringLiteral("QFrame#mamMessageActions { background-color: palette(base);"
                                                  " border: 1px solid palette(mid); border-radius: 5px; }"));
    messageActions_->installEventFilter(this);
    auto *toolbar = new QHBoxLayout(messageActions_);
    toolbar->setContentsMargins(3, 2, 3, 2);
    toolbar->setSpacing(0);
    const auto makeActionButton = [this](QWidget *parent, const QString &objectName, const QString &tooltip) {
        auto *button = new QPushButton(parent);
        button->setObjectName(objectName);
        button->setFlat(true);
        button->setFixedSize(28, 28);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tooltip);
        button->setAccessibleName(tooltip);
        button->installEventFilter(this);
        return button;
    };

    quickReactions_ = new QWidget(messageActions_);
    quickReactions_->setObjectName(QStringLiteral("mamQuickReactions"));
    quickReactions_->setMinimumWidth(0);
    quickReactions_->setMaximumWidth(0);
    quickReactions_->installEventFilter(this);
    auto *quickLayout = new QHBoxLayout(quickReactions_);
    // The 1 px right margin separates the slot from the react button and
    // exists only while the slot is expanded.
    quickLayout->setContentsMargins(0, 0, 1, 0);
    quickLayout->setSpacing(1);
    for (const auto &value : { QString::fromUtf8("👍"), QString::fromUtf8("🔥"), QString::fromUtf8("❤️") }) {
        auto *button = makeActionButton(quickReactions_, QStringLiteral("mamQuickReaction"), value);
        button->setText(value);
        quickLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, value] {
            const int at = indexOfKey(actionKey_);
            if (at >= 0)
                toggleReaction(reactionTarget(items_.at(at).message), value);
            setQuickReactionsExpanded(false);
        });
    }
    toolbar->addWidget(quickReactions_);
    quickAnimation_ = new QPropertyAnimation(quickReactions_, "maximumWidth", this);
    quickAnimation_->setDuration(110);
    connect(quickAnimation_, &QPropertyAnimation::valueChanged, this, &MamChatWidget::positionMessageActions);
    quickHideTimer_ = new QTimer(this);
    quickHideTimer_->setSingleShot(true);
    quickHideTimer_->setInterval(180);
    connect(quickHideTimer_, &QTimer::timeout, this, [this] { setQuickReactionsExpanded(false); });

    auto *buttons       = new QWidget(messageActions_);
    auto *buttonsLayout = new QHBoxLayout(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(1);
    toolbar->addWidget(buttons);

    // React is the leftmost ordinary action: the quick reactions open next
    // to it and must not appear over another button.
    chooseReaction_ = makeActionButton(buttons, QStringLiteral("mamChooseReaction"), tr("Add reaction"));
    const QPixmap smile = IconsetFactory::iconPixmap(QStringLiteral("psi/smile"), QSize(18, 18));
    if (smile.isNull()) {
        chooseReaction_->setText(QString::fromUtf8("☺"));
    } else {
        chooseReaction_->setIcon(QIcon(smile));
        chooseReaction_->setIconSize(QSize(18, 18));
    }
    buttonsLayout->addWidget(chooseReaction_);
    connect(chooseReaction_, &QPushButton::clicked, this, [this] {
        const int at = indexOfKey(actionKey_);
        if (at >= 0)
            emit emojiChooserRequested(reactionTarget(items_.at(at).message),
                                       chooseReaction_->mapToGlobal(QPoint(0, chooseReaction_->height())));
    });

    moreActions_ = makeActionButton(buttons, QStringLiteral("mamMoreActions"), tr("More actions"));
    moreActions_->setText(QString::fromUtf8("⋯"));
    buttonsLayout->addWidget(moreActions_);
    messageMenu_ = new QMenu(this);
    messageMenu_->setObjectName(QStringLiteral("mamMessageMenu"));
    auto *retract = messageMenu_->addAction(tr("Retract message"));
    retract->setObjectName(QStringLiteral("mamRetractMessage"));
    connect(moreActions_, &QPushButton::clicked, this, [this] {
        // The overlay may hide while the menu is open; act on the row that
        // opened it, not on whatever is hovered later.
        messageMenu_->setProperty("_psiMamRowKey", actionKey_);
        messageMenu_->popup(moreActions_->mapToGlobal(QPoint(0, moreActions_->height())));
    });
    connect(retract, &QAction::triggered, this, [this] {
        const int at = indexOfKey(messageMenu_->property("_psiMamRowKey").toString());
        if (at < 0 || !items_.at(at).message.isLocal() || isRetracted(items_.at(at)))
            return;
        const QString id = retractionTarget(items_.at(at).message);
        if (id.isEmpty())
            return;
        emit        outgoingMessageRetraction(id);
        MessageView event(MessageView::MessageRetraction);
        event.setLocal(true);
        event.setNick(items_.at(at).message.nick());
        event.setRetractionId(id);
        appendMessage(event);
        hideMessageActions();
    });

    actionsOpacity_ = new QGraphicsOpacityEffect(messageActions_);
    actionsOpacity_->setOpacity(1.0);
    messageActions_->setGraphicsEffect(actionsOpacity_);
    actionsFade_ = new QPropertyAnimation(actionsOpacity_, "opacity", this);
    actionsFade_->setDuration(120);
    connect(actionsFade_, &QPropertyAnimation::finished, this, [this] {
        if (actionsFade_->endValue().toReal() <= 0.0)
            hideMessageActions();
    });
    messageActions_->hide();
    actionHideTimer_ = new QTimer(this);
    actionHideTimer_->setSingleShot(true);
    actionHideTimer_->setInterval(160);
    connect(actionHideTimer_, &QTimer::timeout, this, [this] {
        if (!messageActions_->isVisible() || messageActions_->underMouse())
            return;
        actionsFade_->stop();
        actionsFade_->setStartValue(actionsOpacity_->opacity());
        actionsFade_->setEndValue(0.0);
        actionsFade_->start();
    });
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setObjectName(QStringLiteral("mamCopyShortcut"));
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    copyShortcut->setEnabled(false); // Single-message QLabel Ctrl+C stays intact.
    connect(copyShortcut, &QShortcut::activated, this, &MamChatWidget::copySelectedMessages);
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &MamChatWidget::cancelMessageSelection);

    connect(this, &Mattermost::LongListWidget::userScrollStarted, this, [this] {
        followingInitialTail_ = false;
        hideMessageActions();
    });
    // actionTriggered is a user-directed scrollbar action, including an
    // outward PageUp/StepUp at value == minimum (which cannot decrease).
    // Programmatic changes during prepends/layout never emit this signal.
    auto *bar = verticalScrollBar();
    connect(bar, &QScrollBar::actionTriggered, this, [this, bar](int action) {
        const bool upward = action == QAbstractSlider::SliderSingleStepSub
            || action == QAbstractSlider::SliderPageStepSub
            || action == QAbstractSlider::SliderToMinimum
            || (action == QAbstractSlider::SliderMove && bar->sliderPosition() < bar->value());
        if (!upward)
            return;
        const quint64 serial = historyRequestSerial_;
        QTimer::singleShot(0, this, [this, bar, serial] {
            if (historyRequestSerial_ != serial)
                return; // Another handler already consumed this action.
            const auto visible = visibleRange();
            if (bar->value() == bar->minimum() || (visible.isValid() && visible.first <= 1))
                requestOlderMessages("scrollbar");
        });
    });

    connect(this, &Mattermost::LongListWidget::visibleRangeChanged, this, [this](int, int) {
        // Visibility is a geometry fact, not authorization to fetch more MAM.
        updateTimestamps();
    });
    connect(this, &Mattermost::LongListWidget::materializedRangeChanged, this,
            [this](int, int) { updateTimestamps(); });
}

namespace {
// QLayout's preferred height uses its preferred width. For a wrapped document
// that width can be much narrower than the timeline, inflating the whole row.
// LongListWidget measures at the viewport width, so report the same height here.
class ChatMessageRow : public QFrame {
public:
    QSize sizeHint() const override { return QSize(0, qMax(1, heightForWidth(qMax(1, width())))); }
};

bool validateLayout(const QJsonValue &node, QSet<QString> &seen, int depth = 0)
{
    if (depth > 8)
        return false;
    if (node.isString()) {
        const QString slot = node.toString();
        if (slot != QLatin1String("author") && slot != QLatin1String("timestamp") && slot != QLatin1String("avatar")
            && slot != QLatin1String("body"))
            return false;
        if (seen.contains(slot))
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

QJsonObject resolvedLayout(const NativeChatTheme &theme)
{
    if (!theme.layoutSpec.isEmpty()) {
        const auto    custom = QJsonDocument::fromJson(theme.layoutSpec.toUtf8());
        QSet<QString> seen;
        if (custom.isObject() && validateLayout(custom.object(), seen) && seen.contains(QStringLiteral("body")))
            return custom.object();
    }
    static const auto modern
        = QJsonDocument::fromJson(
              R"({"direction":"horizontal","items":["avatar",{"direction":"vertical","items":[{"direction":"horizontal","items":["author","timestamp"]},"body"]}]})")
              .object();
    static const auto classic
        = QJsonDocument::fromJson(R"({"direction":"horizontal","items":["timestamp","author","body"]})").object();
    return theme.layout == NativeChatTheme::Classic ? classic : modern;
}

QBoxLayout *buildThemeLayout(const QJsonObject &node, const QHash<QString, QWidget *> &elements,
                             const NativeChatTheme &theme)
{
    const bool  horizontal = node.value(QStringLiteral("direction")).toString() == QLatin1String("horizontal");
    QBoxLayout *layout
        = horizontal ? static_cast<QBoxLayout *>(new QHBoxLayout) : static_cast<QBoxLayout *>(new QVBoxLayout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(horizontal ? (theme.layout == NativeChatTheme::Classic ? 4 : 10) : 3);
    for (const auto &item : node.value(QStringLiteral("items")).toArray()) {
        if (item.isObject()) {
            auto *nested = buildThemeLayout(item.toObject(), elements, theme);
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

}

void MamChatWidget::scrollToIndex(int index, Mattermost::LongListWidget::Alignment alignment)
{
    followingInitialTail_ = false;
    Mattermost::LongListWidget::scrollToIndex(index, alignment);
}

bool MamChatWidget::sameSender(const MessageView &a, const MessageView &b) const
{
    // The widget is scoped to one conversation. A direct-chat sender can use
    // several resources or display names, whereas a MUC nick is the sender.
    return a.isLocal() == b.isLocal() && (!groupChat_ || a.nick() == b.nick());
}

int MamChatWidget::findRelatedMessage(const MessageView &incoming, bool allowHistory) const
{
    if (incoming.type() != MessageView::Message)
        return -1;
    for (int i = items_.size() - 1; i >= 0; --i) {
        const auto &row      = items_.at(i);
        const auto &previous = row.message;
        if (previous.type() != MessageView::Message || !sameSender(previous, incoming))
            continue;
        if (allowHistory && incoming.historyId() > 0 && incoming.historyId() == previous.historyId())
            return i;

        // Explicit correction edges connect revisions even when the target
        // was fetched before or after the correction, or hours earlier.
        if (!incoming.replaceId().isEmpty() && row.stanzaIds.contains(incoming.replaceId()))
            return i;
        if (!incoming.messageId().isEmpty() && row.stanzaIds.contains(incoming.messageId())) {
            if (incoming.messageId() != previous.messageId())
                return i; // Already proven to be an earlier revision.
            // Bare client stanza IDs aren't verified archive identities.
            // Limit same-version matching to a nearby delivery interval.
            if (PsiMam::sameDelivery(
                    { incoming.messageId(), incoming.formattedText(), incoming.isLocal(), incoming.dateTime() },
                    { previous.messageId(), previous.formattedText(), previous.isLocal(), previous.dateTime() }))
                return i;
        }
    }
    return -1;
}

void MamChatWidget::mergeMessage(int index, const MessageView &incoming, const QString &sourceKey)
{
    Row       &row      = items_[index];
    const auto previous = row.message;
    const bool edited   = previous.isEdited() || incoming.isEdited() || !previous.replaceId().isEmpty()
        || !incoming.replaceId().isEmpty();
    if (!previous.messageId().isEmpty())
        row.stanzaIds.insert(previous.messageId());
    if (!previous.replaceId().isEmpty())
        row.stanzaIds.insert(previous.replaceId());
    if (!incoming.messageId().isEmpty())
        row.stanzaIds.insert(incoming.messageId());
    if (!incoming.replaceId().isEmpty())
        row.stanzaIds.insert(incoming.replaceId());

    // An archive fetch after a live edit must not overwrite the latest
    // revision with the old text or the raw JID nick from history.
    const bool projectionChanged = !sourceKey.isEmpty() && incoming.messageId() == previous.messageId()
        && incoming.revisionTime() == previous.revisionTime()
        && (incoming.formattedText() != previous.formattedText()
            || incoming.formattedUserText() != previous.formattedUserText());
    const bool newer = projectionChanged
        || (incoming.revisionTime().isValid()
            && (!previous.revisionTime().isValid() || incoming.revisionTime() > previous.revisionTime()
                || (incoming.messageId() != previous.messageId() && incoming.replaceId() == previous.messageId())));
    // Reaction/retraction targets name the original message. A live MUC row
    // may lack the room-assigned stanza-id that its archived copy carries, so
    // learn a missing target from any revision without replacing a known one.
    const QString reactionId   = !previous.reactionTargetId().isEmpty() ? previous.reactionTargetId()
                                                                        : incoming.reactionTargetId();
    const QString retractionId = !previous.retractionTargetId().isEmpty() ? previous.retractionTargetId()
                                                                          : incoming.retractionTargetId();
    if (newer) {
        MessageView updated = incoming;
        updated.setDateTime(previous.dateTime()); // Editing never moves the original row.
        updated.setReactionTargetId(reactionId);
        updated.setRetractionTargetId(retractionId);
        // Keep a known encryption state (e.g. the live copy) over an unknown one.
        if (!updated.isSecurityKnown() && previous.isSecurityKnown())
            updated.setEncrypted(previous.isEncrypted(), previous.encryptionProtocol());
        updated.setEdited(edited);
        if (!groupChat_ && !previous.nick().isEmpty())
            updated.setNick(previous.nick());
        if (updated.archiveId().isEmpty())
            updated.setArchiveId(previous.archiveId());
        if (updated.historyId() <= 0)
            updated.setHistoryId(previous.historyId());
        row.message = updated;
        replaceItem(index);
    } else if ((edited && !previous.isEdited()) || reactionId != previous.reactionTargetId()
               || retractionId != previous.retractionTargetId()
               || (!previous.isSecurityKnown() && incoming.isSecurityKnown())) {
        if (!previous.isSecurityKnown() && incoming.isSecurityKnown())
            row.message.setEncrypted(incoming.isEncrypted(), incoming.encryptionProtocol());
        row.message.setEdited(edited);
        row.message.setReactionTargetId(reactionId);
        row.message.setRetractionTargetId(retractionId);
        replaceItem(index);
    }

    // Multiple MAM archive UIDs (one per edit) and any local EDB key belong
    // to this one row; never replace its stable selection/scroll anchor key.
    if (!sourceKey.isEmpty() && sourceKey != row.key) {
        keys_.insert(sourceKey);
        sourceAliases_.insert(sourceKey, row.key);
    }
}

void MamChatWidget::coalesceRevisionRows()
{
    // Connecting an earlier EDB original and a live correction may require
    // merging two rows when intermediate edits arrive out of order. Index
    // stanza IDs once instead of comparing every row against every other row
    // on a long history (linear in rows and revision IDs).
    QHash<QString, int> seen;
    for (int i = 0; i < items_.size();) {
        const Row &candidate = items_.at(i);
        if (candidate.message.type() != MessageView::Message) {
            ++i;
            continue;
        }
        const QString sender = (candidate.message.isLocal() ? QStringLiteral("local:") : QStringLiteral("remote:"))
            + (groupChat_ ? candidate.message.nick() : QString());
        const auto identityKey = [&sender](const QString &id) { return sender + QLatin1Char('\x1f') + id; };

        int keeper = -1;
        for (const QString &id : candidate.stanzaIds) {
            const auto it = seen.constFind(identityKey(id));
            if (it == seen.cend())
                continue;
            const int prior = it.value();
            if (prior < i && (items_.at(prior).message.isEdited() || candidate.message.isEdited())) {
                keeper = prior;
                break;
            }
        }

        if (keeper < 0) {
            for (const QString &id : candidate.stanzaIds)
                seen.insert(identityKey(id), i);
            ++i;
            continue;
        }

        const QString survivingKey   = items_.at(keeper).key;
        const QString removedKey     = candidate.key;
        Row           merged         = items_.at(keeper);
        const auto    originalTime   = merged.message.dateTime();
        const auto    originalTarget = !merged.message.reactionTargetId().isEmpty()
               ? merged.message.reactionTargetId()
               : candidate.message.reactionTargetId();
        const auto    retractTarget  = !merged.message.retractionTargetId().isEmpty()
             ? merged.message.retractionTargetId()
             : candidate.message.retractionTargetId();
        merged.stanzaIds.unite(candidate.stanzaIds);
        if (candidate.message.revisionTime().isValid()
            && (!merged.message.revisionTime().isValid()
                || candidate.message.revisionTime() > merged.message.revisionTime()))
            merged.message = candidate.message;
        if (originalTime.isValid())
            merged.message.setDateTime(originalTime);
        merged.message.setReactionTargetId(originalTarget);
        merged.message.setRetractionTargetId(retractTarget);
        merged.message.setEdited();
        items_[keeper] = merged;

        for (auto it = sourceAliases_.begin(); it != sourceAliases_.end(); ++it)
            if (it.value() == removedKey)
                it.value() = survivingKey;
        sourceAliases_.insert(removedKey, survivingKey);
        if (selectedKeys_.remove(removedKey))
            selectedKeys_.insert(survivingKey);
        if (selectionAnchorKey_ == removedKey)
            selectionAnchorKey_ = survivingKey;
        if (pressedKey_ == removedKey)
            pressedKey_ = survivingKey;

        // These indices are all before i; removing i never invalidates them.
        for (const QString &id : merged.stanzaIds)
            seen.insert(identityKey(id), keeper);
        items_.removeAt(i);
        removeItems(i, 1);
        replaceItem(keeper);
        // Re-check the same index: it now contains the next logical row.
    }
}

QString MamChatWidget::reactionTarget(const MessageView &view) const
{
    return !view.reactionTargetId().isEmpty() ? view.reactionTargetId() : groupChat_ ? QString() : view.messageId();
}

QString MamChatWidget::retractionTarget(const MessageView &view) const
{
    // XEP-0424: a direct chat retracts message@id, which may differ from the
    // origin-id used by reactions. A MUC uses the room-assigned stanza-id.
    if (!view.retractionTargetId().isEmpty())
        return view.retractionTargetId();
    return groupChat_ ? QString() : view.messageId();
}

bool MamChatWidget::isRetracted(const Row &row) const
{
    // Also accept the reaction target: earlier XEP-0424 revisions and some
    // clients address retractions by origin-id.
    auto events = retractions_.value(retractionTarget(row.message));
    if (reactionTarget(row.message) != retractionTarget(row.message))
        events.append(retractions_.value(reactionTarget(row.message)));
    for (const auto &id : row.stanzaIds)
        events.append(retractions_.value(id));
    for (const auto &event : events) {
        if (event.isLocal() != row.message.isLocal())
            continue;
        if (groupChat_ && event.nick() != row.message.nick())
            continue;
        if (!groupChat_ || (!event.userId().isEmpty() && event.userId() == row.message.userId()))
            return true;
        // Locally requested retractions have already been tied to their row.
        if (event.isLocal() && event.userId().isEmpty())
            return true;
    }
    return false;
}

int MamChatWidget::displayedTargetIndex() const
{
    return lastDisplayedRowKey_.isEmpty() ? -1 : indexOfKey(lastDisplayedRowKey_);
}

void MamChatWidget::resolveDisplayedMarkers()
{
    if (unresolvedDisplayedIds_.isEmpty())
        return;

    int highest = displayedTargetIndex();
    int changed = -1;
    for (auto it = unresolvedDisplayedIds_.begin(); it != unresolvedDisplayedIds_.end();) {
        int target = -1;
        for (int i = 0; i < items_.size(); ++i) {
            if (items_.at(i).message.isLocal() && items_.at(i).stanzaIds.contains(*it)) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            ++it;
            continue;
        }
        if (target > highest) {
            highest = target;
            lastDisplayedRowKey_ = items_.at(target).key;
            changed = highest;
        }
        it = unresolvedDisplayedIds_.erase(it);
    }
    if (changed >= 0) {
        for (const int index : materializedIndices()) {
            if (index <= changed && items_.at(index).message.isLocal())
                replaceItem(index);
        }
    }
}

QString MamChatWidget::takeVisibleMarkableIncomingId()
{
    if (!isVisible() || window()->isMinimized() || groupChat_)
        return {};
    const auto range = visibleRange();
    if (!range.isValid())
        return {};
    const int previouslyMarked = lastMarkedIncomingRowKey_.isEmpty() ? -1 : indexOfKey(lastMarkedIncomingRowKey_);
    for (int i = range.last; i >= range.first; --i) {
        if (previouslyMarked >= i)
            break; // XEP-0333 markers advance only.
        const auto &message = items_.at(i).message;
        if (message.type() != MessageView::Message || message.isLocal() || !message.isMarkable()
            || message.messageId().isEmpty())
            continue;
        auto *row = itemWidget(i);
        if (row && row->isVisibleTo(viewport())
            && QRect(row->mapTo(viewport(), QPoint()), row->size()).intersects(viewport()->rect()))
        {
            lastMarkedIncomingRowKey_ = items_.at(i).key;
            return message.messageId();
        }
    }
    return {};
}

bool MamChatWidget::applyProtocolEvent(const MessageView &event)
{
    QString target;
    if (event.type() == MessageView::MessageSecurity) {
        // Outgoing encryption outcome; it may arrive before the local row.
        if (event.messageId().isEmpty() || !event.isSecurityKnown())
            return true;
        target = event.messageId();
        security_.insert(target, qMakePair(event.isEncrypted(), event.encryptionProtocol()));
    } else if (event.type() == MessageView::DeliveryReceipt) {
        if (event.isLocal())
            return true;
        target = event.messageId();
        receipts_.insert(target);
    } else if (event.type() == MessageView::DisplayedMarker) {
        if (event.isLocal() || groupChat_ || event.messageId().isEmpty())
            return true;
        // MAM can deliver a marker before the referenced older page. Do not
        // claim any message was read until its id is actually known. Keep only
        // a small bounded set of unresolved references for later pages.
        if (!unresolvedDisplayedIds_.contains(event.messageId())) {
            unresolvedDisplayedIds_.append(event.messageId());
            if (unresolvedDisplayedIds_.size() > 64)
                unresolvedDisplayedIds_.removeFirst();
        }
        resolveDisplayedMarkers();
        return true;
    } else if (event.type() == MessageView::Reactions) {
        target              = event.reactionsId();
        const QString actor = event.isLocal() ? QStringLiteral("self")
            : groupChat_                      ? event.userId().isEmpty() ? event.nick() : event.userId()
                                              : QStringLiteral("peer");
        const auto    saved = reactionEvents_.value(target).value(actor).message;
        if (reactionEvents_.value(target).contains(actor)
            && (saved.dateTime() > event.dateTime()
                || (saved.dateTime() == event.dateTime() && !saved.isSpooled() && event.isSpooled())))
            return true;
        reactionEvents_[target][actor].message = event;
    } else if (event.type() == MessageView::MessageRetraction) {
        target = event.retractionId();
        retractions_[target].append(event);
    } else
        return false;
    hideMessageActions();
    for (int i = 0; i < items_.size(); ++i)
        if (reactionTarget(items_.at(i).message) == target || retractionTarget(items_.at(i).message) == target
            || items_.at(i).stanzaIds.contains(target))
            replaceItem(i);
    return true;
}

void MamChatWidget::toggleReaction(const QString &target, const QString &emoji)
{
    if (target.isEmpty() || emoji.isEmpty())
        return;
    auto set = reactionEvents_.value(target).value(QStringLiteral("self")).message.reactions();
    if (set.contains(emoji))
        set.remove(emoji);
    else
        set.insert(emoji);
    MessageView event(MessageView::Reactions);
    event.setLocal(true);
    event.setReactionsId(target);
    event.setReactions(set);
    emit outgoingReactions(target, set);
    appendMessage(event);
}

void MamChatWidget::hideMessageActions()
{
    if (actionHideTimer_)
        actionHideTimer_->stop();
    if (actionsFade_)
        actionsFade_->stop();
    if (quickHideTimer_)
        quickHideTimer_->stop();
    if (quickAnimation_)
        quickAnimation_->stop();
    if (quickReactions_)
        quickReactions_->setMaximumWidth(0);
    if (messageActions_)
        messageActions_->hide();
    actionKey_.clear();
}

void MamChatWidget::showMessageActions(QFrame *row)
{
    if (!row || !selectedKeys_.isEmpty() || mousePressed_ || selectingByDrag_)
        return;
    const int at = indexOfKey(row->property("_psiMamRowKey").toString());
    if (at < 0 || items_.at(at).message.type() != MessageView::Message || isRetracted(items_.at(at)))
        return;
    const auto &message    = items_.at(at).message;
    const bool  canReact   = !reactionTarget(message).isEmpty();
    const bool  canRetract = message.isLocal() && !retractionTarget(message).isEmpty();
    if (!canReact && !canRetract)
        return;
    actionHideTimer_->stop();
    const bool sameRow = messageActions_->isVisible() && actionKey_ == items_.at(at).key;
    if (!sameRow) {
        quickHideTimer_->stop();
        quickAnimation_->stop();
        quickReactions_->setMaximumWidth(0);
    }
    actionKey_ = items_.at(at).key;
    chooseReaction_->setVisible(canReact);
    moreActions_->setVisible(canRetract);
    positionMessageActions();
    if (!messageActions_->isVisible()) {
        actionsOpacity_->setOpacity(0.0);
        messageActions_->show();
    }
    messageActions_->raise();
    if (actionsFade_->state() != QAbstractAnimation::Running || actionsFade_->endValue().toReal() < 1.0) {
        actionsFade_->stop();
        actionsFade_->setStartValue(actionsOpacity_->opacity());
        actionsFade_->setEndValue(1.0);
        actionsFade_->start();
    }
}

void MamChatWidget::positionMessageActions()
{
    const int at  = indexOfKey(actionKey_);
    auto     *row = at >= 0 ? itemWidget(at) : nullptr;
    if (!row)
        return;
    messageActions_->adjustSize();
    // Float across the row's top edge at its right side, clamped to the view.
    const QPoint rowTopLeft = row->mapTo(viewport(), QPoint(0, 0));
    const int    maxX       = qMax(0, viewport()->width() - messageActions_->width() - 4);
    const int    maxY       = qMax(0, viewport()->height() - messageActions_->height());
    messageActions_->move(qBound(0, rowTopLeft.x() + row->width() - messageActions_->width() - 8, maxX),
                          qBound(0, rowTopLeft.y() - messageActions_->height() / 2, maxY));
}

void MamChatWidget::setQuickReactionsExpanded(bool expanded)
{
    quickHideTimer_->stop();
    const int end = expanded ? quickReactions_->layout()->sizeHint().width() : 0;
    if (quickReactions_->maximumWidth() == end)
        return;
    quickAnimation_->stop();
    quickAnimation_->setStartValue(quickReactions_->maximumWidth());
    quickAnimation_->setEndValue(end);
    quickAnimation_->setEasingCurve(expanded ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    quickAnimation_->start();
}

void MamChatWidget::appendMessage(const MessageView &view)
{
    if (applyProtocolEvent(view))
        return;
    if (((view.type() == MessageView::MUCJoin || view.type() == MessageView::MUCPart) && view.isJoinLeaveHidden())
        || (view.type() == MessageView::Status && view.isStatusChangeHidden()))
        return;

    if (view.historyId() > 0) {
        prependArchivePage({ qMakePair(view.archiveId(), view) });
        return;
    }

    // A newly sent local message is an explicit navigation intent: go to the
    // live tail even if the user was reading older history. Editing an old
    // message must NOT change the viewport.
    const bool sentNewMessage
        = view.type() == MessageView::Message && view.isLocal() && !view.isSpooled() && view.replaceId().isEmpty();
    if (view.type() == MessageView::Message) {
        const int existing = findRelatedMessage(view, false);
        if (existing >= 0) {
            mergeMessage(existing, view);
            resolveDisplayedMarkers();
            if (sentNewMessage)
                scrollToEnd();
            return;
        }
    }

    const QString key = !view.archiveId().isEmpty()
        ? QStringLiteral("mam:") + view.archiveId()
        : QStringLiteral("live:generated:") + QString::number(++generatedKey_);
    if (keys_.contains(key))
        return;

    const bool followTail = followingInitialTail_ || isAtEnd() || sentNewMessage;
    keys_.insert(key);
    Row row { key, view, {} };
    if (!view.messageId().isEmpty())
        row.stanzaIds.insert(view.messageId());
    if (!view.replaceId().isEmpty())
        row.stanzaIds.insert(view.replaceId());
    const int at = items_.size();
    items_.append(row);
    insertItems(at, 1);
    setRangeAvailable(at, at, true);
    resolveDisplayedMarkers();
    if (followTail)
        scrollToEnd();
}

int MamChatWidget::prependArchivePage(const QList<QPair<QString, MessageView>> &page)
{
    const bool followTail         = followingInitialTail_ || isAtEnd();
    int        added              = 0;
    int        floor              = 0;
    bool       containsCorrection = false;
    for (const auto &entry : page)
        containsCorrection |= entry.second.isEdited();
    // Reconcile revisions backwards FIRST: the live row might be revision 8,
    // while this historical page starts at revision 6. Doing only an
    // ascending pass inserts 6 before learning from 7 that it belongs to 8.
    QSet<QString> preMergedKeys;
    for (auto it = page.crbegin(); it != page.crend(); ++it) {
        const auto &incoming = it->second;
        if (incoming.type() != MessageView::Message || (it->first.isEmpty() && incoming.historyId() <= 0))
            continue;
        const QString key = !it->first.isEmpty() ? QStringLiteral("mam:") + it->first
                                                 : QStringLiteral("edb:") + QString::number(incoming.historyId());
        if (keys_.contains(key))
            continue;
        const int existing = findRelatedMessage(incoming, true);
        if (existing < 0)
            continue;
        mergeMessage(existing, incoming, key);
        preMergedKeys.insert(key);
    }
    // Then insert genuinely new messages in chronological page order.
    // Pre-merged corrections must not move the ordering floor to a live
    // tail row, otherwise older unrelated messages land after the tail.
    for (const auto &entry : page) {
        const auto &incoming = entry.second;
        if (applyProtocolEvent(incoming))
            continue;
        if (entry.first.isEmpty() && incoming.historyId() <= 0)
            continue;
        const QString key = !entry.first.isEmpty() ? QStringLiteral("mam:") + entry.first
                                                   : QStringLiteral("edb:") + QString::number(incoming.historyId());
        if (keys_.contains(key)) {
            const int existing = indexOfKey(key);
            if (existing >= 0 && incoming.type() == MessageView::Message)
                mergeMessage(existing, incoming, key);
            if (existing >= 0 && !preMergedKeys.contains(key))
                floor = qMax(floor, existing + 1);
            continue;
        }

        const int existing = findRelatedMessage(incoming, true);
        if (existing >= 0) {
            mergeMessage(existing, incoming, key);
            floor = qMax(floor, existing + 1);
            continue;
        }
        int at = floor;
        while (at < items_.size() && items_.at(at).message.dateTime() < incoming.dateTime())
            ++at;
        keys_.insert(key);
        Row row { key, incoming, {} };
        if (!incoming.messageId().isEmpty())
            row.stanzaIds.insert(incoming.messageId());
        if (!incoming.replaceId().isEmpty())
            row.stanzaIds.insert(incoming.replaceId());
        items_.insert(at, row);
        insertItems(at, 1, InsertViewportPolicy::PreserveVisibleContent);
        setRangeAvailable(at, at, true);
        floor = at + 1;
        ++added;
    }
    if (containsCorrection)
        coalesceRevisionRows();
    resolveDisplayedMarkers();
    if (followTail && !page.isEmpty())
        scrollToEnd();
    return added;
}

void MamChatWidget::setMediaOpener(FileSharingDeviceOpener *opener)
{
    if (mediaOpener_ == opener)
        return;
    mediaOpener_ = opener;
    for (int i = 0; i < items_.size(); ++i)
        if (itemWidget(i))
            replaceItem(i);
}

void MamChatWidget::clearMessages()
{
    hideMessageActions();
    reactionEvents_.clear();
    retractions_.clear();
    receipts_.clear();
    security_.clear();
    lastDisplayedRowKey_.clear();
    lastMarkedIncomingRowKey_.clear();
    unresolvedDisplayedIds_.clear();
    cancelMessageSelection();
    items_.clear();
    keys_.clear();
    sourceAliases_.clear();
    // A plain click may leave an anchor without a selected range. Reset it
    // when changing conversations, including keys reused by the next model.
    selectionAnchorKey_.clear();
    mousePressed_        = false;
    selectingByDrag_     = false;
    swallowMouseRelease_ = false;
    pressedKey_.clear();
    generatedKey_         = 0;
    followingInitialTail_ = true;
    historyLoading_       = false;
    historyExhausted_     = false;
    historyRequestPending_ = false;
    ++historyRequestSerial_; // Invalidate deferred gestures from the old conversation.
    initialFillEligible_ = false;
    initialFillFinished_ = false;
    initialFillCheckPending_ = false;
    initialFillRequests_ = 0;
    setItemCount(0);
    timestampTimer_->stop();
}

void MamChatWidget::requestOlderMessages(const char *gesture)
{
    if (historyLoading_ || historyExhausted_ || historyRequestPending_)
        return;
    historyRequestPending_ = true;
    ++historyRequestSerial_;
    const auto visible = visibleRange();
    qInfo().noquote() << "[mam-history] scrollback demand"
                      << "gesture=" << gesture << "widget=" << this
                      << "atEnd=" << isAtEnd() << "visible=" << visible.first << visible.last
                      << "scroll=" << verticalScrollBar()->value()
                      << "min=" << verticalScrollBar()->minimum()
                      << "max=" << verticalScrollBar()->maximum()
                      << "rows=" << messageCount()
                      << "serial=" << historyRequestSerial_;
    emit olderMessagesRequested();
}

void MamChatWidget::maybeContinueInitialHistoryFill()
{
    // Called only after a successfully handled MAM/EDB page. The first MUC
    // history burst is preserved; only missing older rows are prepended.
    initialFillEligible_ = true;
    scheduleInitialHistoryFillCheck();
}

void MamChatWidget::scheduleInitialHistoryFillCheck()
{
    if (!initialFillEligible_ || initialFillFinished_ || initialFillCheckPending_
        || !followingInitialTail_ || historyLoading_ || historyExhausted_)
        return;
    initialFillCheckPending_ = true;
    // Let LongListWidget materialize and measure real widget heights first.
    QTimer::singleShot(25, this, [this] {
        initialFillCheckPending_ = false;
        if (!initialFillEligible_ || initialFillFinished_ || !followingInitialTail_
            || historyLoading_ || historyExhausted_ || !isVisible() || viewport()->height() <= 0)
            return;
        const bool underfilled = contentHeight() <= viewport()->height();
        qInfo().noquote() << "[mam-history] initial viewport check"
                          << "rows=" << messageCount() << "height=" << contentHeight()
                          << "viewport=" << viewport()->height()
                          << "underfilled=" << underfilled
                          << "followUps=" << initialFillRequests_
                          << "atEnd=" << isAtEnd();
        if (!underfilled) {
            initialFillFinished_ = true;
            return;
        }
        // A room archive may consist entirely of protocol-only MAM events.
        // Limit implicit prefetch independently of explicit user gestures.
        constexpr int MaxInitialFillFollowUps = 6;
        if (initialFillRequests_ >= MaxInitialFillFollowUps) {
            initialFillFinished_ = true;
            return;
        }
        ++initialFillRequests_;
        requestOlderMessages("initial-viewport-fill");
    });
}

void MamChatWidget::setHistoryLoading(bool loading)
{
    // Completion is not a scrollback request. In particular, a room archive
    // can contain many protocol-only records without filling the viewport.
    historyLoading_ = loading;
    if (loading)
        historyRequestPending_ = false;
}

void MamChatWidget::setHistoryExhausted(bool exhausted)
{
    historyExhausted_ = exhausted;
    if (exhausted)
        historyRequestPending_ = false;
}

QWidget *MamChatWidget::createItemWidget(int index)
{
    const auto &mv  = items_.at(index).message;
    auto       *row = new ChatMessageRow;
    row->setFrameShape(QFrame::NoFrame);
    row->setObjectName(QStringLiteral("mamMessageRow"));
    row->setProperty("_psiMamRowKey", items_.at(index).key);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    row->setFont(font());
    row->setProperty("_psiMamInformational", mv.type() != MessageView::Message);
    // Only the timestamp owns the full date/time tooltip.
    row->installEventFilter(this);

    row->setStyleSheet(theme_.styleSheet);
    auto      *outer   = new QHBoxLayout(row);
    const bool modern  = theme_.layout == NativeChatTheme::Modern;
    const bool message = mv.type() == MessageView::Message;
    outer->setContentsMargins(12, modern ? 8 : 2, 12, modern ? 8 : 2);
    outer->setSpacing(theme_.spacing);
    auto *selection = new QCheckBox(row);
    selection->setObjectName(QStringLiteral("mamMessageSelection"));
    selection->setToolTip(tr("Select message"));
    selection->setAccessibleName(tr("Select message"));
    selection->setVisible(!selectedKeys_.isEmpty());
    selection->setChecked(selectedKeys_.contains(items_.at(index).key));
    outer->addWidget(selection, 0, Qt::AlignTop);
    connect(selection, &QCheckBox::toggled, this, [this, row](bool checked) {
        // A prepend shifts indices, and a MAM overlap can adopt a new key.
        // Resolve the row's current identity at the time of the click.
        const QString key   = row->property("_psiMamRowKey").toString();
        const int     index = indexOfKey(key);
        if (index >= 0) {
            if (checked)
                selectionAnchorKey_ = key;
            setMessageSelected(index, checked);
        }
    });

    auto *content = new QWidget(row);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    outer->addWidget(content, 1, Qt::AlignTop);
    QHash<QString, QWidget *> elements;

    if (message) {
        const QString nick   = mv.nick().isEmpty() ? (mv.isLocal() ? tr("Me") : tr("Message")) : mv.nick();
        auto         *author = new QLabel(modern ? nick : nick + QLatin1Char(':'), content);
        author->setObjectName(QStringLiteral("mamAuthor"));
        author->setTextFormat(Qt::PlainText);
        author->setTextInteractionFlags(Qt::TextSelectableByMouse);
        author->setForegroundRole(QPalette::Text);
        QFont nameFont = author->font();
        nameFont.setBold(modern);
        author->setFont(nameFont);
        elements.insert(QStringLiteral("author"), author);
        author->installEventFilter(this);
        auto *time
            = new QLabel(modern ? displayTime(mv.dateTime())
                                : QStringLiteral("[") + mv.dateTime().toLocalTime().toString(QStringLiteral("HH:mm:ss"))
                                 + QStringLiteral("]"),
                         content);
        time->setObjectName(QStringLiteral("mamTimestamp"));
        time->setTextFormat(Qt::PlainText);
        const bool read = mv.isLocal() && !groupChat_ && displayedTargetIndex() >= index;
        bool delivered = false;
        if (mv.isLocal() && !groupChat_) {
            // A receipt may reference any revision of a corrected message.
            // The visible row represents one logical message, not only the
            // latest revision's stanza id.
            for (const auto &id : items_.at(index).stanzaIds) {
                if (receipts_.contains(id)) {
                    delivered = true;
                    break;
                }
            }
        }
        time->setToolTip(fullTimestamp(mv.dateTime()));
        time->setForegroundRole(QPalette::Text);
        time->installEventFilter(this);
        // Encryption state of the message, possibly learnt after it was
        // sent (the encryption job finishes asynchronously).
        bool    securityKnown = mv.isSecurityKnown();
        bool    encrypted     = mv.isEncrypted();
        QString protocol      = mv.encryptionProtocol();
        if (!securityKnown) {
            for (const auto &id : items_.at(index).stanzaIds) {
                const auto it = security_.constFind(id);
                if (it != security_.cend()) {
                    securityKnown = true;
                    encrypted     = it->first;
                    protocol      = it->second;
                    break;
                }
            }
        }
        if (mv.isEdited() || read || delivered || securityKnown) {
            // One semantic timestamp slot: time, delivery/read state, edit mark.
            // Do not append glyphs to the time QLabel itself: its text is
            // refreshed for relative-time display and themes can reposition it.
            auto *timeGroup = new QWidget(content);
            timeGroup->setObjectName(QStringLiteral("mamTimestampGroup"));
            auto *timeLayout = new QHBoxLayout(timeGroup);
            timeLayout->setContentsMargins(0, 0, 0, 0);
            timeLayout->setSpacing(5);
            timeLayout->addWidget(time);
            if (read || delivered) {
                auto *checkmarks = new DeliveryIndicator(read, time, timeGroup);
                const QString status = read ? tr("Read") : tr("Delivered");
                checkmarks->setToolTip(status);
                checkmarks->setAccessibleName(status);
                timeLayout->addWidget(checkmarks, 0, Qt::AlignVCenter);
            }
            if (securityKnown) {
                auto *lock = new IconMark(encrypted ? QStringLiteral("psi/cryptoYes") : QStringLiteral("psi/cryptoNo"),
                                          time, timeGroup);
                lock->setObjectName(QStringLiteral("mamEncryptionIndicator"));
                lock->setProperty("encrypted", encrypted);
                const QString status = !encrypted   ? tr("Not encrypted")
                    : protocol.isEmpty()            ? tr("Encrypted")
                                                    : tr("Encrypted with %1").arg(protocol);
                lock->setToolTip(status);
                lock->setAccessibleName(status);
                timeLayout->addWidget(lock, 0, Qt::AlignVCenter);
            }
            if (mv.isEdited()) {
            // Psi's edit icon at the shared timestamp-mark size and baseline.
            auto *pencil = new IconMark(QStringLiteral("psi/action_templates_edit"), time, timeGroup);
            pencil->setObjectName(QStringLiteral("mamEditedIndicator"));
            pencil->setToolTip(tr("The message was corrected"));
            pencil->setAccessibleName(tr("Edited message"));
            timeLayout->addWidget(pencil, 0, Qt::AlignVCenter);
            }
            elements.insert(QStringLiteral("timestamp"), timeGroup);
        } else {
            elements.insert(QStringLiteral("timestamp"), time);
        }
        if (theme_.avatarSize > 0) {
            auto *avatar = new QLabel(content);
            avatar->setObjectName(QStringLiteral("mamAvatar"));
            avatar->setCursor(Qt::PointingHandCursor);
            avatar->setFixedSize(theme_.avatarSize, theme_.avatarSize);
            updateAvatar(avatar, mv);
            elements.insert(QStringLiteral("avatar"), avatar);
            avatar->installEventFilter(this);
        }
        updateNicknameColor(author, mv);
        updateTimestampAppearance(time);
    }

    auto *body = new ChatMessageText(content);
    body->setObjectName(QStringLiteral("mamBody"));
    body->setMediaOpener(mediaOpener_);
    body->setFont(font());
    body->setForegroundRole(QPalette::Text);
    const bool retracted = isRetracted(items_.at(index));
    QString    richText  = retracted ? tr("Message retracted") : mv.formattedText();
    // XEP-0245 is a presentation rule, not a wire transformation. MessageView
    // has already stripped "/me " but retains isEmote(); restore the sender
    // prefix for the native widget log, just as the legacy ChatView did.
    if (!retracted && mv.isEmote()) {
        const QString actor = mv.nick().isEmpty() ? tr("Contact") : mv.nick();
        richText = QStringLiteral("*") + actor.toHtmlEscaped() + QStringLiteral(" ") + richText;
    }
    if (!retracted && !mv.formattedUserText().isEmpty())
        richText += (richText.isEmpty() ? QString() : QStringLiteral(" — ")) + mv.formattedUserText();
    body->setRichText(richText);
    elements.insert(QStringLiteral("body"), body);
    body->installEventFilter(this);
    body->viewport()->installEventFilter(this);

    // As in Bubble, each actor sends its entire current set, not an add/remove
    // delta. Chips aggregate actors and toggling a chip updates our own set.
    const QString              target = reactionTarget(mv);
    QMap<QString, QStringList> counts;
    for (const auto &event : reactionEvents_.value(target))
        for (const auto &emoji : event.message.reactions())
            counts[emoji].append(event.message.isLocal()              ? tr("Me")
                                     : event.message.nick().isEmpty() ? tr("Contact")
                                                                      : event.message.nick());
    if (!retracted && !counts.isEmpty()) {
        auto *group  = new QWidget(content);
        auto *column = new QVBoxLayout(group);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);
        column->addWidget(body);
        auto *chips = new QWidget(group);
        chips->setObjectName(QStringLiteral("mamReactions"));
        auto *strip = new QHBoxLayout(chips);
        strip->setContentsMargins(0, 0, 0, 0);
        strip->setSpacing(4);
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            auto *chip = new QPushButton(it.key() + QStringLiteral(" %1").arg(it.value().size()), chips);
            chip->setObjectName(QStringLiteral("mamReactionChip"));
            // MatterLeast's ReactionChipStyle: neutral translucent background,
            // compact font-relative height, small radius and whole-chip target.
            chip->setFixedHeight(qMax(22, fontMetrics().height() + 6));
            chip->setCursor(Qt::PointingHandCursor);
            // Our own reaction uses a light translucent green, as in Telegram,
            // which stays readable on both light and dark themes.
            chip->setStyleSheet(QStringLiteral("QPushButton#mamReactionChip { border: 1px solid rgba(128,128,128,130);"
                                               " border-radius:4px; background:rgba(128,128,128,26); padding:0px 4px; }"
                                               "QPushButton#mamReactionChip:hover { background:rgba(128,128,128,36); }"
                                               "QPushButton#mamReactionChip:checked { border-color:rgba(76,175,80,170);"
                                               " background:rgba(76,175,80,70); }"
                                               "QPushButton#mamReactionChip:checked:hover {"
                                               " background:rgba(76,175,80,95); }"));
            chip->setToolTip(it.value().join(QLatin1Char('\n')));
            chip->setCheckable(true);
            chip->setChecked(
                reactionEvents_.value(target).value(QStringLiteral("self")).message.reactions().contains(it.key()));
            connect(chip, &QPushButton::clicked, this,
                    [this, target, emoji = it.key()] { toggleReaction(target, emoji); });
            strip->addWidget(chip);
        }
        strip->addStretch();
        column->addWidget(chips);
        elements[QStringLiteral("body")] = group;
    }

    if (modern && theme_.layoutSpec.isEmpty()) {
        // Keep Psi's original Modern presentation geometry intact. Qt6 treats
        // nested QBoxLayouts differently from Qt5 when the body has a custom
        // height-for-width and the header is not a separate QWidget.
        if (auto *avatar = elements.value(QStringLiteral("avatar"), nullptr))
            outer->insertWidget(1, avatar, 0, Qt::AlignTop);
        auto *column = new QVBoxLayout(content);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);
        if (message) {
            auto *header = new QWidget(content);
            header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(0, 0, 0, 0);
            headerLayout->setSpacing(10);
            headerLayout->addWidget(elements.value(QStringLiteral("author")));
            headerLayout->addWidget(elements.value(QStringLiteral("timestamp")));
            headerLayout->addStretch();
            column->addWidget(header, 0, Qt::AlignTop);
        }
        column->addWidget(elements.value(QStringLiteral("body")), 0, Qt::AlignTop);
    } else {
        // Classic and plugin themes can choose any nested row/column
        // arrangement without owning or replacing the message widget.
        const QJsonObject layout = resolvedLayout(theme_);
        content->setLayout(buildThemeLayout(layout, elements, theme_));
        QSet<QString> used;
        validateLayout(layout, used);
        for (auto it = elements.cbegin(); it != elements.cend(); ++it)
            if (!used.contains(it.key()))
                it.value()->hide();
    }
    if (!message) {
        QTextCursor cursor(body->document());
        cursor.select(QTextCursor::Document);
        QTextBlockFormat format;
        format.setAlignment(mv.type() == MessageView::Subject ? Qt::AlignLeft : Qt::AlignHCenter);
        cursor.mergeBlockFormat(format);
    }
    updateRowSelection(row, selectedKeys_.contains(items_.at(index).key));
    return row;
}

QString MamChatWidget::displayTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return {};
    const qint64 age = now.toMSecsSinceEpoch() - when.toMSecsSinceEpoch();
    if (age < 0 || age >= 24 * 60 * 60 * 1000)
        return when.toLocalTime().toString(QStringLiteral("HH:mm"));
    const int seconds = int(age / 1000);
    if (seconds < 60)
        return tr("just now");
    if (seconds < 3600)
        return (seconds / 60 == 1 ? tr("1 minute ago") : tr("%n minutes ago", nullptr, seconds / 60));
    return (seconds / 3600 == 1 ? tr("1 hour ago") : tr("%n hours ago", nullptr, seconds / 3600));
}

QString MamChatWidget::fullTimestamp(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    const auto local = when.toLocalTime();
    return QLocale().toString(local.date(), QLocale::LongFormat) + QStringLiteral(" ")
        + local.toString(QStringLiteral("HH:mm:ss t"));
}

void MamChatWidget::updateTimestampAppearance(QLabel *label)
{
    // Always derive the timestamp font from the CHAT font, never the label's
    // previous font: recreating a row or switching themes must not progressively
    // shrink it. Classic keeps the full-size legacy [HH:mm:ss] presentation.
    QFont      timeFont = font();
    const bool classic  = theme_.layout == NativeChatTheme::Classic;
    if (!classic) {
        if (timeFont.pointSizeF() > 0)
            timeFont.setPointSizeF(qMax(7.0, timeFont.pointSizeF() - 1.0));
        else if (timeFont.pixelSize() > 0)
            timeFont.setPixelSize(qMax(9, timeFont.pixelSize() - 1));
    }
    label->setFont(timeFont);

    QColor color = palette().color(QPalette::Text);
    if (classic && groupChat_) {
        // The QTextEdit-based MUC Classic painted [time] and nickname with
        // the SAME nick color, including informational/uncolored history.
        if (auto *author = label->parentWidget()->findChild<QLabel *>(QStringLiteral("mamAuthor")))
            color = author->palette().color(QPalette::Text);
    } else if (!classic) {
        color.setAlpha(188); // The Modern time stays legible but unobtrusive.
    }
    QPalette colors = label->palette();
    colors.setColor(QPalette::Text, color);
    colors.setColor(QPalette::WindowText, color);
    label->setPalette(colors);
    // Timestamp marks have no Q_OBJECT, so Qt6's findChild<T>() rejects
    // them; find the sibling QLabels and cast with RTTI instead.
    for (auto *sibling : label->parentWidget()->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (auto *mark = dynamic_cast<TimestampMark *>(sibling)) {
            mark->setPalette(colors);
            mark->setTimestampFont(timeFont);
        }
    }
}

void MamChatWidget::updateTimestamps()
{
    timestampTimer_->stop();
    // Hidden tabs/windows and Classic's absolute timestamps require no clock.
    if (!isVisible() || window()->isMinimized() || theme_.layout == NativeChatTheme::Classic)
        return;

    const auto range = visibleRange();
    if (!range.isValid())
        return;
    const auto   now         = QDateTime::currentDateTime();
    const qint64 minute      = 60 * 1000;
    const qint64 hour        = 60 * minute;
    const qint64 day         = 24 * hour;
    qint64       nextRefresh = std::numeric_limits<int>::max();

    // The prefetch/materialization buffer can be much larger than the
    // viewport. Touch only rows that actually intersect the visible area.
    for (int i = range.first; i <= range.last; ++i) {
        auto *row = itemWidget(i);
        if (!row || !row->isVisibleTo(viewport())
            || !QRect(row->mapTo(viewport(), QPoint()), row->size()).intersects(viewport()->rect()))
            continue;
        auto *label = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        if (!label)
            continue;
        const auto when = items_.at(i).message.dateTime();
        if (!when.isValid())
            continue;
        const auto text = displayTime(when, now);
        if (label->text() != text)
            label->setText(text);

        const qint64 age       = when.msecsTo(now);
        qint64       untilNext = 0;
        if (age < 0)
            untilNext = -age; // Future timestamp becomes "just now".
        else if (age < hour)
            untilNext = minute - age % minute;
        else if (age < day)
            untilNext = hour - age % hour;
        // Beyond 24 hours we show a fixed clock time, with no timer.
        if (untilNext > 0)
            nextRefresh = qMin(nextRefresh, untilNext + 50);
    }
    if (nextRefresh < std::numeric_limits<int>::max())
        timestampTimer_->start(int(nextRefresh));
}

void MamChatWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange)
        schedulePaletteRefresh();
    Mattermost::LongListWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        for (int index : materializedIndices()) {
            auto *row = itemWidget(index);
            if (!row)
                continue;
            row->setFont(font());
            if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"))) {
                QFont authorFont = font();
                authorFont.setBold(theme_.layout == NativeChatTheme::Modern);
                author->setFont(authorFont);
            }
            if (auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
                updateTimestampAppearance(time);
            if (auto *body = row->findChild<ChatMessageText *>())
                body->setFont(font());
        }
        if (!items_.isEmpty())
            itemsChanged(0, items_.size() - 1);
    } else if (event->type() == QEvent::PaletteChange) {
        refreshPalette();
        emit paletteChanged();
    }
}

void MamChatWidget::schedulePaletteRefresh()
{
    if (paletteRefreshPending_)
        return;
    paletteRefreshPending_ = true;
    QTimer::singleShot(0, this, [this] {
        paletteRefreshPending_ = false;
        // TabDlg reapplies its stylesheet on application palette changes.
        // Qt's stylesheet repolish restores cached palettes on existing rows
        // after our synchronous PaletteChange handler has already updated them.
        refreshPalette();
    });
}

void MamChatWidget::refreshPalette()
{
    // With a stylesheet, the viewport need not inherit the configured chat
    // colors. Resolve rows from the chat palette, including custom link colors.
    viewport()->setPalette(palette());
    refreshSelection();
    refreshNicknameColors();
    refreshAvatars();
    for (int index : materializedIndices())
        if (auto *row = itemWidget(index))
            if (auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
                updateTimestampAppearance(time);
}

void MamChatWidget::setTheme(const NativeChatTheme &theme)
{
    theme_ = theme;
    for (int index : materializedIndices())
        replaceItem(index);
    updateTimestamps();
}

void MamChatWidget::setNicknameColorResolver(std::function<QColor(const MessageView &)> resolver)
{
    nicknameColorResolver_ = std::move(resolver);
    refreshNicknameColors();
}

void MamChatWidget::refreshNicknameColors()
{
    for (int index : materializedIndices()) {
        if (auto *row = itemWidget(index))
            if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor")))
                updateNicknameColor(author, items_.at(index).message);
    }
}

void MamChatWidget::updateNicknameColor(QLabel *author, const MessageView &message)
{
    // Even an uncolored nickname needs an explicit refresh: a stylesheet on
    // the chat window can leave this nested QLabel with its old resolved palette.
    const QColor color   = nicknameColorResolver_ ? nicknameColorResolver_(message) : QColor();
    QPalette     palette = author->palette();
    palette.setColor(QPalette::Text, color.isValid() ? color : this->palette().color(QPalette::Text));
    palette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
    author->setPalette(palette);
    if (auto *time = author->parentWidget()->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
        updateTimestampAppearance(time);
}

void MamChatWidget::setAvatarResolver(std::function<QPixmap(const MessageView &)> resolver)
{
    avatarResolver_ = std::move(resolver);
    refreshAvatars();
}

void MamChatWidget::updateAvatar(QLabel *label, const MessageView &message)
{
    const auto source = avatarResolver_ ? avatarResolver_(message) : QPixmap();
    const int  size   = label->width();
    const auto ratio  = devicePixelRatioF();
    QPixmap    picture(qRound(size * ratio), qRound(size * ratio));
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), 6, 6);
    painter.setClipPath(clip);
    if (source.isNull()) {
        painter.fillRect(QRect(0, 0, size, size), palette().brush(QPalette::Highlight));
        painter.setPen(palette().color(QPalette::HighlightedText));
        QFont initial = font();
        initial.setPixelSize(qMax(12, size / 2));
        painter.setFont(initial);
        const QString nick = message.nick().isEmpty() ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, nick.left(1).toUpper());
    } else {
        const auto  scaled = source.scaled(picture.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRect crop((scaled.width() - picture.width()) / 2, (scaled.height() - picture.height()) / 2,
                         picture.width(), picture.height());
        painter.drawPixmap(QRect(0, 0, size, size), scaled, crop);
    }
    painter.end();
    label->setPixmap(picture);
}

void MamChatWidget::refreshAvatars()
{
    for (int i : materializedIndices()) {
        if (auto *row = itemWidget(i))
            if (auto *avatar = row->findChild<QLabel *>(QStringLiteral("mamAvatar")))
                updateAvatar(avatar, items_.at(i).message);
    }
}

int MamChatWidget::indexOfKey(const QString &key) const
{
    if (key.isEmpty())
        return -1;
    const QString canonical = sourceAliases_.value(key, key);
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).key == canonical)
            return i;
    }
    return -1;
}

QFrame *MamChatWidget::eventRow(QObject *target) const
{
    QWidget *child = qobject_cast<QWidget *>(target);
    while (child && child != viewport()) {
        if (auto *frame = qobject_cast<QFrame *>(child)) {
            if (frame->objectName() == QLatin1String("mamMessageRow"))
                return frame;
        }
        child = child->parentWidget();
    }
    return nullptr;
}

void MamChatWidget::selectMessageRange(int first, int last)
{
    if (first < 0 || last < 0 || first >= items_.size() || last >= items_.size())
        return;
    if (first > last)
        qSwap(first, last);
    selectedKeys_.clear();
    for (int i = first; i <= last; ++i)
        selectedKeys_.insert(items_.at(i).key);
    refreshSelection();
}

void MamChatWidget::setMessageSelected(int index, bool selected)
{
    if (index < 0 || index >= items_.size())
        return;
    const QString &key = items_.at(index).key;
    if (selected)
        selectedKeys_.insert(key);
    else
        selectedKeys_.remove(key);
    refreshSelection();
}

void MamChatWidget::cancelMessageSelection()
{
    if (selectedKeys_.isEmpty() && !selectionToolbar_->isVisible())
        return;
    selectedKeys_.clear();
    selectionAnchorKey_.clear();
    selectingByDrag_ = false;
    refreshSelection();
}

void MamChatWidget::copySelectedMessages()
{
    if (selectedKeys_.isEmpty())
        return;
    // Model-backed copying works even when LongListWidget evicted the row UI.
    QStringList blocks;
    for (const auto &row : items_) {
        if (!selectedKeys_.contains(row.key))
            continue;
        const auto   &message = row.message;
        QTextDocument document;
        const bool    retracted = isRetracted(row);
        QString       rich      = retracted ? tr("Message retracted") : message.formattedText();
        if (!retracted && message.isEmote()) {
            const QString actor = message.nick().isEmpty() ? tr("Contact") : message.nick();
            rich = QStringLiteral("*") + actor.toHtmlEscaped() + QStringLiteral(" ") + rich;
        }
        if (!retracted && !message.formattedUserText().isEmpty())
            rich += (rich.isEmpty() ? QString() : QStringLiteral(" — ")) + message.formattedUserText();
        PsiRichText::setText(&document, rich);
        const QString nick = message.nick().isEmpty() ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
        const QString timestamp = message.dateTime().isValid()
            ? message.dateTime().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();
        blocks.append(
            QStringLiteral("%1\t[%2]\n%3").arg(nick, timestamp, PsiRichText::convertToPlainText(&document).trimmed()));
    }
    if (!blocks.isEmpty())
        QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n\n")));
}

void MamChatWidget::setInformationalColor(const QColor &color)
{
    informationalColor_ = color;
    refreshSelection();
}

void MamChatWidget::updateRowSelection(QFrame *row, bool selected)
{
    const bool selecting = !selectedKeys_.isEmpty();
    if (auto *check = row->findChild<QCheckBox *>(QStringLiteral("mamMessageSelection"))) {
        const QSignalBlocker blocker(check);
        check->setChecked(selected);
        check->setVisible(selecting);
    }
    const QColor base    = viewport()->palette().color(QPalette::Base);
    const QColor accent  = viewport()->palette().color(QPalette::Highlight);
    const QColor surface = selected
        ? QColor((base.red() * 3 + accent.red()) / 4, (base.green() * 3 + accent.green()) / 4,
                 (base.blue() * 3 + accent.blue()) / 4)
        : base;
    QPalette     palette = viewport()->palette();
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::Base, surface); // QTextDocument's surface, too.
    row->setPalette(palette);
    row->setAutoFillBackground(true);
    if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"))) {
        author->setTextInteractionFlags(selecting ? Qt::NoTextInteraction : Qt::TextSelectableByMouse);
        if (selecting)
            author->setSelection(0, 0);
    }
    if (auto *body = row->findChild<ChatMessageText *>()) {
        QPalette textPalette = palette;
        if (row->property("_psiMamInformational").toBool())
            textPalette.setColor(QPalette::Text,
                                 informationalColor_.isValid() ? informationalColor_ : palette.color(QPalette::Link));
        body->setPalette(textPalette);
        body->setWholeMessageSelectionMode(selecting);
    }
    row->update();
}

void MamChatWidget::refreshSelection()
{
    // Only paint materialized rows. Selection itself is stored independently
    // and automatically applied by createItemWidget after virtualization.
    const auto range = materializedRange();
    if (range.isValid()) {
        for (int i = range.first; i <= range.last; ++i) {
            auto *row = qobject_cast<QFrame *>(itemWidget(i));
            if (!row || row->objectName() != QLatin1String("mamMessageRow"))
                continue;
            updateRowSelection(row, selectedKeys_.contains(items_.at(i).key));
        }
    }
    if (auto *shortcut = findChild<QShortcut *>(QStringLiteral("mamCopyShortcut")))
        shortcut->setEnabled(!selectedKeys_.isEmpty());
    if (selectedKeys_.isEmpty()) {
        selectionToolbar_->hide();
        return;
    }
    selectionCountLabel_->setText(tr("%1 selected").arg(selectedKeys_.size()));
    positionSelectionToolbar();
    selectionToolbar_->show();
    selectionToolbar_->raise();
}

void MamChatWidget::positionSelectionToolbar()
{
    if (!selectionToolbar_)
        return;
    selectionToolbar_->adjustSize();
    selectionToolbar_->move(qMax(8, (viewport()->width() - selectionToolbar_->width()) / 2), 8);
}

void MamChatWidget::wheelEvent(QWheelEvent *event)
{
    // Delegate actual scroll geometry/anchors to the shared MatterLeast list.
    // Unlike visibleRangeChanged, an upward wheel is an explicit demand. Test
    // AFTER scrolling so a gesture reaching the oldest visible row counts too.
    const int delta = event->pixelDelta().y() != 0 ? event->pixelDelta().y()
                                                    : event->angleDelta().y();
    Mattermost::LongListWidget::wheelEvent(event);
    if (delta <= 0)
        return;
    const auto visible = visibleRange();
    if (verticalScrollBar()->value() == verticalScrollBar()->minimum()
        || (visible.isValid() && visible.first <= 1))
        requestOlderMessages("wheel");
}

void MamChatWidget::resizeEvent(QResizeEvent *event)
{
    Mattermost::LongListWidget::resizeEvent(event);
    // The first layout pass may run while the room is still hidden; restoring
    // its old geometric anchor must never cancel the initial at-end intent.
    if (followingInitialTail_ && !items_.isEmpty())
        scrollToEnd();
    positionSelectionToolbar();
    scheduleInitialHistoryFillCheck();
}

void MamChatWidget::showEvent(QShowEvent *event)
{
    Mattermost::LongListWidget::showEvent(event);
    // Revisit after Qt has measured the splitter/viewport on first display.
    // Any intervening user scroll has already released the initial tail.
    QTimer::singleShot(0, this, [this] {
        if (followingInitialTail_ && !items_.isEmpty())
            scrollToEnd();
        scheduleInitialHistoryFillCheck();
        updateTimestamps();
    });
}

void MamChatWidget::hideEvent(QHideEvent *event)
{
    timestampTimer_->stop();
    Mattermost::LongListWidget::hideEvent(event);
}

bool MamChatWidget::eventFilter(QObject *target, QEvent *event)
{
    auto      *widget  = qobject_cast<QWidget *>(target);
    const bool hover   = event->type() == QEvent::Enter || event->type() == QEvent::Leave;
    const bool inQuick = hover && widget && (widget == quickReactions_ || quickReactions_->isAncestorOf(widget));
    if (event->type() == QEvent::Enter) {
        if (target == chooseReaction_) {
            actionHideTimer_->stop();
            setQuickReactionsExpanded(true);
        } else if (inQuick) {
            quickHideTimer_->stop();
        } else if (target == messageActions_ || (widget && messageActions_->isAncestorOf(widget))) {
            actionHideTimer_->stop();
            if (actionsFade_->state() == QAbstractAnimation::Running && actionsFade_->endValue().toReal() <= 0.0) {
                actionsFade_->stop();
                actionsFade_->setStartValue(actionsOpacity_->opacity());
                actionsFade_->setEndValue(1.0);
                actionsFade_->start();
            }
        } else if (auto *row = eventRow(target))
            showMessageActions(row);
    } else if (event->type() == QEvent::Leave) {
        if (target == chooseReaction_ || inQuick)
            quickHideTimer_->start();
        if (target == messageActions_ || eventRow(target))
            actionHideTimer_->start();
    } else if (event->type() == QEvent::MouseButtonRelease && target == chooseReaction_) {
        // Do not change the toolbar geometry between the press and release
        // of the react button, or QAbstractButton may not emit clicked().
        QTimer::singleShot(0, this, [this] { setQuickReactionsExpanded(false); });
    }

    if (event
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove
            || event->type() == QEvent::MouseButtonRelease)) {
        if (QFrame *row = eventRow(target)) {
            auto         *mouse = static_cast<QMouseEvent *>(event);
            const QString key   = row->property("_psiMamRowKey").toString();
            const int     index = indexOfKey(key);
            if (index >= 0) {
                if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
                    hideMessageActions();
                    const bool extended = mouse->modifiers().testFlag(Qt::ShiftModifier);
                    const bool toggle   = mouse->modifiers().testFlag(Qt::ControlModifier);
                    if (extended || toggle || !selectedKeys_.isEmpty()) {
                        const int anchor = indexOfKey(selectionAnchorKey_);
                        if (extended && anchor >= 0)
                            selectMessageRange(anchor, index);
                        else
                            setMessageSelected(index, !selectedKeys_.contains(key));
                        if (!extended || selectionAnchorKey_.isEmpty())
                            selectionAnchorKey_ = key;
                        swallowMouseRelease_ = true;
                        mousePressed_        = false;
                        return true;
                    }
                    // Do not steal drag-selection of text *within* a message.
                    pressedKey_         = key;
                    selectionAnchorKey_ = key;
                    mousePressed_       = true;
                } else if (event->type() == QEvent::MouseMove && mousePressed_) {
                    // The press/release lifecycle belongs to this view. Qt5's
                    // synthetic mouse-move delivery can omit buttons() even
                    // while the row holds a live left-button press.

                    auto        *source = qobject_cast<QWidget *>(target);
                    const QPoint where  = source->mapTo(viewport(), mouse->pos());
                    const QRect  rect(row->mapTo(viewport(), QPoint()), row->size());
                    if (!selectingByDrag_ && where.y() >= rect.top() && where.y() <= rect.bottom())
                        return Mattermost::LongListWidget::eventFilter(target, event);
                    const int anchor = indexOfKey(pressedKey_);
                    const int now    = indexAtViewportPosition(where.y());
                    if (anchor >= 0 && now >= 0) {
                        selectingByDrag_ = true;
                        selectMessageRange(anchor, now);
                        return true;
                    }
                } else if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton) {
                    mousePressed_        = false;
                    const bool consumed  = selectingByDrag_ || swallowMouseRelease_;
                    selectingByDrag_     = false;
                    swallowMouseRelease_ = false;
                    if (consumed)
                        return true;
                    if (selectedKeys_.isEmpty() && target->objectName() == QLatin1String("mamAvatar")) {
                        auto *avatar = qobject_cast<QLabel *>(target);
                        if (avatar && avatar->rect().contains(mouse->pos())) {
                            const auto &sender = items_.at(index).message;
                            emit        avatarClicked(sender.nick(), sender.isLocal());
                            return true;
                        }
                    }
                    if (groupChat_ && selectedKeys_.isEmpty() && target->objectName() == QLatin1String("mamAuthor")) {
                        auto *author = qobject_cast<QLabel *>(target);
                        if (author && !author->hasSelectedText() && author->rect().contains(mouse->pos())) {
                            emit nicknameClicked(items_.at(index).message.nick());
                        }
                    }
                }
            }
        }
    }
    return Mattermost::LongListWidget::eventFilter(target, event);
}
