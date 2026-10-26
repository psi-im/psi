/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamChatWidget.h"
#include "MamImageViewer.h"
#include "ChatMessageText.h"
#include "MessagePreview.h"
#include "filesharingmanager.h"
#include "fileutil.h"
#include "urlobject.h"
#include <QContextMenuEvent>
#include <QCursor>
#include "MamHistoryLog.h"
#include "MamMessageIdentity.h"
#include "iconset.h"
#include "psirichtext.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImageReader>
#include <QDebug>
#include <QFontInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
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
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrlQuery>
#include <QTextBlockFormat>
#include <QTextImageFormat>
#include <QTextFragment>
#include <QTextDocument>
#include <QWheelEvent>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <optional>

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
    // Native chat logs should meet the splitter edge without the default
    // QAbstractScrollArea frame, in both private and group conversations.
    setFrameShape(QFrame::NoFrame);
    qRegisterMetaType<ChatReply>();
    qRegisterMetaType<MamHistorySource::Request>();
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
    qCDebug(PsiMam::historyLog).noquote() << "timeline-created build=history-demand-v4 widget=" << this;
    setDefaultItemHeight(72);
    setMaterializationLimit(160);
    setRequestBlockSize(40);
    // Keep one semantic hover owner across row gaps and the viewport-owned
    // action bar, as MatterLeast does. Opaque row/text surfaces need the same
    // highlight color as the overlay painted by LongListWidget underneath.
    viewport()->installEventFilter(this);

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
                toggleReaction(reactionKey(items_.at(at)), value);
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
            emit emojiChooserRequested(reactionKey(items_.at(at)),
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

    // Scrolling (e.g. with the wheel) while the left button is held moves rows
    // under a stationary pointer without any MouseMove. Re-evaluate the drag
    // selection so the rows scrolled under the cursor are selected too. This
    // runs after LongListWidget has laid out the new scroll position.
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        if (!mousePressed_ || !QGuiApplication::mouseButtons().testFlag(Qt::LeftButton))
            return;
        updateDragSelection(viewport()->mapFromGlobal(QCursor::pos()));
    });

    connect(this, &Mattermost::LongListWidget::userScrollStarted, this, [this] {
        ++archiveIntentSerial_;
        followingInitialTail_ = false;
        rangeDemandAllowed_      = true;
        historyFillTargetPixels_ = -1; // A gesture cancels the previous fill demand.
        hideMessageActions();
    });
    // actionTriggered is a user-directed scrollbar action, including an
    // outward PageUp/StepUp at value == minimum (which cannot decrease).
    // Programmatic changes during prepends/layout never emit this signal.
    auto *bar = verticalScrollBar();
    bar->installEventFilter(this); // Wheel/keyboard input can occur at an unmoving minimum.
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

    connect(this, &Mattermost::LongListWidget::rangeRequested, this, &MamChatWidget::requestArchiveRange);
    connect(this, &Mattermost::LongListWidget::visibleRangeChanged, this, [this](int, int) {
        // Visibility is a geometry fact, not authorization to fetch more MAM.
        updateTimestamps();
    });
    connect(this, &Mattermost::LongListWidget::materializedRangeChanged, this, [this](int, int) {
        updateTimestamps();
        scheduleInitialHistoryFillCheck();
    });
}

bool MamChatWidget::updateDragSelection(const QPoint &where)
{
    const int anchor = indexOfKey(pressedKey_);
    if (anchor < 0)
        return false;
    // Until the pointer leaves the pressed row, the drag belongs to text
    // selection inside that message.
    if (!selectingByDrag_) {
        if (QWidget *row = rowWidget(anchor)) {
            const QRect rect(row->mapTo(viewport(), QPoint()), row->size());
            if (where.y() >= rect.top() && where.y() <= rect.bottom())
                return false;
        }
    }
    const int now = historySource_.knownIndex(indexAtViewportPosition(where.y()));
    if (now < 0)
        return false;
    if (!selectingByDrag_) {
        selectingByDrag_ = true;
        // The pressed row holds Qt's implicit mouse grab, but scrolling can
        // evict (hide and delete) it. Its moves and the final release would
        // then be lost. Hold an explicit grab on the viewport for the rest of
        // the gesture instead.
        if (viewport()->isVisible())
            viewport()->grabMouse();
    }
    selectMessageRange(anchor, now);
    return true;
}

void MamChatWidget::finishDragSelection()
{
    mousePressed_        = false;
    selectingByDrag_     = false;
    swallowMouseRelease_ = false;
    if (QWidget::mouseGrabber() == viewport())
        viewport()->releaseMouse();
}

QWidget *MamChatWidget::rowWidget(int at) const
{
    return Mattermost::LongListWidget::itemWidget(historySource_.logicalIndex(at));
}
QVector<int> MamChatWidget::materializedRows() const
{
    QVector<int> result;
    for (int logical : Mattermost::LongListWidget::materializedIndices()) {
        const int at = historySource_.knownIndex(logical);
        if (at >= 0)
            result.append(at);
    }
    return result;
}
Mattermost::LongListWidget::Range MamChatWidget::knownVisibleRange() const
{
    const auto visible = visibleRange();
    if (!visible.isValid())
        return {};
    const auto &positions = historySource_.positions_;
    const auto  first     = std::lower_bound(positions.cbegin(), positions.cend(), visible.first);
    const auto  end       = std::upper_bound(first, positions.cend(), visible.last);
    return first == end ? Range {} : Range { int(first - positions.cbegin()), int(end - positions.cbegin()) - 1 };
}
QString MamChatWidget::itemIdentity(const QWidget *widget) const
{
    return widget ? widget->property("_psiMamRowKey").toString() : QString();
}
int  MamChatWidget::indexOfItemIdentity(const QString &key) const { return historySource_.indexOfKey(key); }
bool MamChatWidget::isModelItemAvailable(int index) const { return historySource_.isAvailable(index); }
void MamChatWidget::replaceRow(int at)
{
    const int logical = historySource_.logicalIndex(at);
    if (logical >= 0)
        Mattermost::LongListWidget::replaceItem(logical);
}
void MamChatWidget::insertHistoryRows(int at, int count, InsertViewportPolicy policy)
{
    if (historySource_.isVirtual())
        syncSourceLayout();
    else {
        historySource_.rebuild();
        Mattermost::LongListWidget::insertItems(at, count, policy);
    }
}
void MamChatWidget::removeHistoryRows(int at, int count)
{
    if (historySource_.isVirtual())
        syncSourceLayout();
    else {
        historySource_.rebuild();
        Mattermost::LongListWidget::removeItems(at, count);
    }
}
void MamChatWidget::syncSourceLayout(bool enableVirtual)
{
    // Let the existing semantic remap transaction carry widgets and measured
    // heights. The source changes only identity -> logical-index mappings.
    for (int at : materializedRows())
        if (const auto *row = rowWidget(at))
            measuredMessageHeights_.insert(row->property("_psiMamRowKey").toString(),
                                           qMakePair(viewport()->width(), qMax(1, row->height())));
    const auto previous = historySource_.positions_;
    if (!followingInitialTail_ && !hasViewportLock()) {
        const int logical  = viewportCenterIndex();
        qreal     fraction = 0;
        if (Mattermost::LongListWidget::itemWidget(logical)
            && captureSemanticAnchorFraction(logical, SemanticAnchorPoint::ItemCenter, fraction))
            lockViewportToSemanticAnchor(logical, SemanticAnchorPoint::ItemCenter, fraction);
    }
    if (enableVirtual) {
        if (!historySource_.isVirtual())
            setDefaultItemHeight(qMax(1, qRound(estimatedAverageMessageHeight())));
        historySource_.enableVirtual();
    } else
        historySource_.rebuild();
    const int count = historySource_.itemCount();
    if (count > itemCount())
        setItemCount(count);
    // A density refinement can shrink estimated geometry below the old seek
    // slot. Move the existing seek to the same event before setItemCount can
    // retire its generation. LongListWidget still owns all geometry/locks.
    if (archiveRangeActive_ && activeArchiveRange_.generation > 0 && count > 0)
        resolveSeekTarget(historySource_.logicalPositionForEvent(historySource_.navigationTarget()),
                          activeArchiveRange_.generation);
    if (itemCount() > 0)
        reconcileItemLayout(0, qMin(1, itemCount() - 1));
    if (count < itemCount())
        setItemCount(count);
    for (int logical : previous)
        if (logical < count && !historySource_.isAvailable(logical))
            setRangeAvailable(logical, logical, false);
    for (int logical : historySource_.positions_)
        setRangeAvailable(logical, logical, true);
    qCDebug(PsiMam::historyLog) << "source-layout known=" << items_.size() << "logical=" << count
                                << "eventCount=" << historySource_.archiveCount()
                                << "visibleRatio=" << historySource_.visibleRatio()
                                << "virtual=" << historySource_.isVirtual();
}
void MamChatWidget::restoreArchiveSnapshot(int count, const QString &lastUid)
{
    historySource_.restoreSnapshot(count, lastUid);
}
void MamChatWidget::observeArchivePage(const QStringList &uids, int count, int firstIndex, const QString &before,
                                       const QString &after, bool tailChain, bool stable)
{
    QStringList rowKeys;
    for (const auto &uid : uids) {
        const int at = indexOfKey(QStringLiteral("mam:") + uid);
        rowKeys.append(at < 0 ? QString() : items_.at(at).key);
    }
    historySource_.observePage(uids, rowKeys, count, firstIndex, before, after, tailChain, stable);
    if (historySource_.isVirtual() || (!followingInitialTail_ || initialFillFinished_))
        syncSourceLayout(true);
}
void MamChatWidget::requestArchiveRange(int first, int last, RequestReason reason, quint64 generation)
{
    if (!historySource_.isVirtual() || !rangeDemandAllowed_) {
        finishRangeRequest(first, last);
        return;
    }
    const auto request = historySource_.requestForRange(first, last, suggestedOlderHistoryPageSize(), generation,
                                                        reason == RequestReason::Seek);
    if (request.kind == MamHistorySource::Request::None) {
        finishRangeRequest(first, last);
        return;
    }
    if (archiveRangeActive_) {
        // Density/layout changes can move the same seek far from its old
        // estimated slots. They join its existing navigation; only a new
        // physical/programmatic intent may cancel it for a different area.
        if (activeArchiveIntent_ == archiveIntentSerial_
            || (first <= activeArchiveRange_.last + 1 && last >= activeArchiveRange_.first - 1)) {
            const auto waiter = qMakePair(first, last);
            if (!archiveRangeWaiters_.contains(waiter))
                archiveRangeWaiters_.append(waiter);
            return;
        }
        emit archiveRangeCancelled();
        for (const auto &waiter : archiveRangeWaiters_)
            finishRangeRequest(waiter.first, waiter.second);
    }
    activeArchiveIntent_ = archiveIntentSerial_;
    archiveNavigationKeys_.clear();
    archiveNavigationPixels_ = 0;
    archiveNavigationGoal_   = qMax(1, viewport()->height() * (reason == RequestReason::Seek ? 3 : 1) / 2);
    historySource_.beginNavigation(request);
    activeArchiveRange_  = request;
    archiveRangeWaiters_ = { qMakePair(first, last) };
    archiveRangeActive_  = true;
    qCDebug(PsiMam::historyLog) << "source-demand range=" << first << last << "kind=" << request.kind
                                << "index=" << request.index << "cursor=" << request.cursor << "limit=" << request.limit
                                << "generation=" << generation;
    emit archiveRangeRequested(request);
}
void MamChatWidget::cancelArchiveRanges()
{
    archiveRangeActive_ = archiveRangePaused_ = false;
    rangeDemandAllowed_                       = false;
    emit       archiveRangeCancelled();
    const auto waiters = archiveRangeWaiters_;
    archiveRangeWaiters_.clear();
    for (const auto &waiter : waiters)
        finishRangeRequest(waiter.first, waiter.second);
    setHistoryLoading(false);
}
void MamChatWidget::completeArchiveRange(const MamHistorySource::Request &request, bool success, const QString &error,
                                         bool queryComplete)
{
    if (!archiveRangeActive_ || request.generation != activeArchiveRange_.generation
        || request.kind != activeArchiveRange_.kind || request.index != activeArchiveRange_.index
        || request.cursor != activeArchiveRange_.cursor)
        return;
    QString failure = error;
    if (success && request.kind == MamHistorySource::Request::Index && !historySource_.lastPageWasIndexed()) {
        success = false;
        failure = tr("Archive response omitted the requested index");
    }
    if (success) {
        for (const auto &key : historySource_.lastPageMessageKeys()) {
            if (archiveNavigationKeys_.contains(key))
                continue;
            archiveNavigationKeys_.insert(key);
            const int   at  = indexOfKey(key);
            const auto *row = at >= 0 ? rowWidget(at) : nullptr;
            archiveNavigationPixels_ += row ? qMax(1, row->height()) : qMax(1, qCeil(estimatedAverageMessageHeight()));
        }
        const auto next = historySource_.continueNavigation(request, queryComplete,
                                                            archiveNavigationPixels_ < archiveNavigationGoal_);
        if (next.kind != MamHistorySource::Request::None) {
            activeArchiveRange_ = next;
            qCDebug(PsiMam::historyLog) << "source-continue protocol-only kind=" << next.kind
                                        << "cursor=" << next.cursor << "generation=" << next.generation;
            if (isVisible())
                emit archiveRangeRequested(next);
            else {
                archiveRangePaused_ = true;
                setHistoryLoading(false);
            }
            return;
        }
    }
    archiveRangeActive_ = false;
    // One gesture authorizes this navigation, including its protocol-only
    // continuations. Layout/materialization afterwards cannot start another
    // independent scan of every still-estimated slot around the viewport.
    rangeDemandAllowed_ = false;
    setHistoryLoading(false);
    if (!success) {
        stopHistoryFill();
        if (request.kind == MamHistorySource::Request::Index) {
            historySource_.rejectIndex();
            syncSourceLayout();
            emit archiveSeekUnavailable(failure);
        }
    } else if (request.generation > 0) {
        const auto key = historySource_.closestKnownKey(historySource_.navigationTarget());
        const int  at  = historySource_.indexOfKey(key);
        if (at >= 0)
            resolveSeekTarget(at, request.generation);
        if (historySource_.navigationStalled())
            qCWarning(PsiMam::historyLog) << "source-stop repeated navigation cursor";
    }
    const auto waiters = archiveRangeWaiters_;
    archiveRangeWaiters_.clear();
    for (const auto &waiter : waiters)
        finishRangeRequest(waiter.first, waiter.second);
}

int MamChatWidget::estimatedItemHeight(int index) const
{
    const int at = historySource_.knownIndex(index);
    if (at >= 0) {
        const auto saved = measuredMessageHeights_.value(items_.at(at).key);
        if (saved.first == viewport()->width() && saved.second > 0)
            return saved.second;
    }
    return defaultItemHeight();
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
    ++archiveIntentSerial_;
    followingInitialTail_ = false;
    rangeDemandAllowed_   = true;
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
        if (!incoming.archiveId().isEmpty() && incoming.archiveId() == previous.archiveId())
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
    const auto messageState = previous.messageState() | incoming.messageState();
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
            || incoming.formattedUserText() != previous.formattedUserText()
            || (incoming.reply().isValid() && (incoming.reply().id != previous.reply().id
                                               || incoming.reply().to != previous.reply().to)));
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
        updated.setMessageState(messageState);
        if (!updated.reply().isValid() && previous.reply().isValid())
            updated.setReply(previous.reply());
        if (!groupChat_ && !previous.nick().isEmpty())
            updated.setNick(previous.nick());
        if (updated.archiveId().isEmpty())
            updated.setArchiveId(previous.archiveId());
        if (updated.historyId() <= 0)
            updated.setHistoryId(previous.historyId());
        row.message = updated;
        replaceRow(index);
    } else if (messageState != previous.messageState() || (edited && !previous.isEdited()) || reactionId != previous.reactionTargetId()
               || retractionId != previous.retractionTargetId()
               || (!previous.isSecurityKnown() && incoming.isSecurityKnown())) {
        if (!previous.isSecurityKnown() && incoming.isSecurityKnown())
            row.message.setEncrypted(incoming.isEncrypted(), incoming.encryptionProtocol());
        row.message.setEdited(edited);
        row.message.setMessageState(messageState);
        row.message.setReactionTargetId(reactionId);
        row.message.setRetractionTargetId(retractionId);
        replaceRow(index);
    }

    // Multiple MAM archive UIDs (one per edit) and any local EDB key belong
    // to this one row; never replace its stable selection/scroll anchor key.
    if (!sourceKey.isEmpty() && sourceKey != row.key) {
        keys_.insert(sourceKey);
        sourceAliases_.insert(sourceKey, row.key);
    }
    if (newer)
        refreshReplyPreviews(true);
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
        const auto    messageState   = merged.message.messageState() | candidate.message.messageState();
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
        merged.message.setMessageState(messageState);
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
        removeHistoryRows(i, 1);
        replaceRow(keeper);
        // Re-check the same index: it now contains the next logical row.
    }
}

QString MamChatWidget::reactionTarget(const MessageView &view) const
{
    return !view.reactionTargetId().isEmpty() ? view.reactionTargetId() : groupChat_ ? QString() : view.messageId();
}

namespace {
// Rooms on servers without XEP-0359 stanza-ids (e.g. old ejabberd without MAM)
// give no XEP-0444 reaction target. As a deliberate, bounded deviation we let
// reactions address message@id, but only within the last few messages, where
// both sides can disambiguate it the same way. Such rows store their reactions
// under a local key, never sent on the wire.
const QString LegacyReactionPrefix = QStringLiteral("legacy-row:");
constexpr int LegacyReactionWindow = 10;
}

QString MamChatWidget::reactionKey(const Row &row) const
{
    const QString target = reactionTarget(row.message);
    if (!target.isEmpty() || !groupChat_ || row.message.type() != MessageView::Message
        || row.message.messageId().isEmpty())
        return target;
    return LegacyReactionPrefix + row.key;
}

int MamChatWidget::legacyReactionTarget(const QString &messageId, const QDateTime &before) const
{
    if (!groupChat_ || messageId.isEmpty())
        return -1;
    // Walk back from the newest message. The first message@id match wins, so
    // a reused id further up can never be addressed by mistake. Messages newer
    // than the reaction itself (e.g. when history is replayed) do not count.
    int seen = 0;
    for (int i = items_.size() - 1; i >= 0 && seen < LegacyReactionWindow; --i) {
        const auto &message = items_.at(i).message;
        if (message.type() != MessageView::Message)
            continue;
        if (before.isValid() && message.dateTime().isValid() && message.dateTime() > before)
            continue;
        ++seen;
        if (message.messageId() == messageId)
            return reactionTarget(message).isEmpty() ? i : -1;
    }
    return -1;
}

bool MamChatWidget::canSendLegacyReaction(int index) const
{
    if (!groupChat_ || index < 0 || index >= items_.size())
        return false;
    const auto &message = items_.at(index).message;
    if (message.type() != MessageView::Message || !reactionTarget(message).isEmpty())
        return false;
    // Only when receivers walking back from their newest message will find
    // this very row first: within the window, and no newer message reuses its id.
    return legacyReactionTarget(message.messageId(), {}) == index;
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
    if (row.message.messageState().testFlag(MessageState::Retracted))
        return true;
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
    for (auto it = unresolvedSentDisplayedIds_.begin(); it != unresolvedSentDisplayedIds_.end();) {
        int target = -1;
        for (int i = 0; i < items_.size(); ++i) {
            const auto &message = items_.at(i).message;
            if (message.type() == MessageView::Message && !message.isLocal() && items_.at(i).stanzaIds.contains(*it)) {
                if (target >= 0) {
                    target = -1; // An ambiguous peer ID is not a read position.
                    break;
                }
                target = i;
            }
        }
        if (target < 0) {
            ++it;
            continue;
        }
        rememberDisplayedIncoming(target);
        it = unresolvedSentDisplayedIds_.erase(it);
    }
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
        for (const int index : materializedRows()) {
            if (index <= changed && items_.at(index).message.isLocal())
                replaceRow(index);
        }
    }
}

void MamChatWidget::restoreDisplayedIncoming(const QString &id, const QDateTime &time, qint64 historyId)
{
    if (groupChat_ || id.isEmpty())
        return;
    if (time.isValid()
        && (!lastMarkedIncomingTime_.isValid() || time > lastMarkedIncomingTime_
            || (time == lastMarkedIncomingTime_ && historyId > lastMarkedIncomingHistoryId_))) {
        lastMarkedIncomingTime_      = time;
        lastMarkedIncomingHistoryId_ = historyId;
    }
    unresolvedSentDisplayedIds_.insert(id);
    resolveDisplayedMarkers();
}

void MamChatWidget::rememberDisplayedIncoming(int index)
{
    const int previous = lastMarkedIncomingRowKey_.isEmpty() ? -1 : indexOfKey(lastMarkedIncomingRowKey_);
    if (index > previous)
        lastMarkedIncomingRowKey_ = items_.at(index).key;
    const auto &message = items_.at(index).message;
    if (message.dateTime().isValid()
        && (!lastMarkedIncomingTime_.isValid() || message.dateTime() > lastMarkedIncomingTime_
            || (message.dateTime() == lastMarkedIncomingTime_ && message.historyId() > lastMarkedIncomingHistoryId_))) {
        lastMarkedIncomingTime_      = message.dateTime();
        lastMarkedIncomingHistoryId_ = message.historyId();
    }
}

QString MamChatWidget::takeVisibleMarkableIncomingId()
{
    if (!isVisible() || window()->isMinimized() || groupChat_)
        return {};
    const auto range = knownVisibleRange();
    if (!range.isValid())
        return {};
    const int previouslyMarked = lastMarkedIncomingRowKey_.isEmpty() ? -1 : indexOfKey(lastMarkedIncomingRowKey_);
    for (int i = range.last; i >= range.first; --i) {
        const auto &message = items_.at(i).message;
        if (message.type() != MessageView::Message || message.isLocal() || !message.isMarkable()
            || message.messageId().isEmpty() || message.messageState().testFlag(MessageState::Displayed))
            continue;
        // Independently loaded pages can insert equal-timestamp rows before
        // the current row. Prefer their durable EDB order when it is known.
        const bool newerHistory = lastMarkedIncomingTime_.isValid() && message.dateTime() == lastMarkedIncomingTime_
            && lastMarkedIncomingHistoryId_ > 0 && message.historyId() > lastMarkedIncomingHistoryId_;
        if (previouslyMarked >= i && !newerHistory)
            continue; // XEP-0333 markers advance only.
        if (unresolvedSentDisplayedIds_.contains(message.messageId())
            || (lastMarkedIncomingTime_.isValid() && message.dateTime().isValid()
                && (message.dateTime() < lastMarkedIncomingTime_
                    || (message.dateTime() == lastMarkedIncomingTime_ && message.historyId() > 0
                        && message.historyId() <= lastMarkedIncomingHistoryId_))))
            continue;
        auto *row = rowWidget(i);
        if (row && row->isVisibleTo(viewport())
            && QRect(row->mapTo(viewport(), QPoint()), row->size()).intersects(viewport()->rect()))
        {
            rememberDisplayedIncoming(i);
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
        if (groupChat_ || event.messageId().isEmpty())
            return true;
        if (event.isLocal()) {
            restoreDisplayedIncoming(event.messageId());
            return true;
        }
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
        target = event.reactionsId();
        if (groupChat_ && !target.isEmpty()) {
            // Row::stanzaIds also holds message@id values, so only the
            // room-assigned reaction target counts as a regular match here.
            bool known = false;
            for (const auto &row : items_) {
                if (reactionTarget(row.message) == target) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                const int at = legacyReactionTarget(target, event.dateTime());
                if (at >= 0)
                    target = reactionKey(items_.at(at));
            }
        }
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
        if (reactionKey(items_.at(i)) == target || retractionTarget(items_.at(i).message) == target
            || items_.at(i).stanzaIds.contains(target))
            replaceRow(i);
    if (event.type() == MessageView::MessageRetraction)
        refreshReplyPreviews(true);
    return true;
}

void MamChatWidget::toggleReaction(const QString &target, const QString &emoji)
{
    if (target.isEmpty() || emoji.isEmpty())
        return;
    QString wireTarget = target;
    if (target.startsWith(LegacyReactionPrefix)) {
        // The row may have left the legacy window since its actions were shown.
        const int at = indexOfKey(target.mid(LegacyReactionPrefix.size()));
        if (!canSendLegacyReaction(at))
            return;
        wireTarget = items_.at(at).message.messageId();
    }
    auto set = reactionEvents_.value(target).value(QStringLiteral("self")).message.reactions();
    if (set.contains(emoji))
        set.remove(emoji);
    else
        set.insert(emoji);
    MessageView event(MessageView::Reactions);
    event.setLocal(true);
    event.setReactionsId(wireTarget);
    event.setReactions(set);
    emit outgoingReactions(wireTarget, set);
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
    const bool  canReact   = !reactionTarget(message).isEmpty() || canSendLegacyReaction(at);
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
    auto     *row = at >= 0 ? rowWidget(at) : nullptr;
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
    insertHistoryRows(at, 1);
    if (!historySource_.isVirtual())
        setRangeAvailable(at, at, true);
    resolveDisplayedMarkers();
    if (followTail)
        scrollToEnd();
    refreshReplyPreviews();
}

int MamChatWidget::prependArchivePage(const QList<QPair<QString, MessageView>> &page)
{
    const bool followTail         = followingInitialTail_ || (isAtEnd() && historyFillTargetPixels_ < 0);
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
        insertHistoryRows(at, 1, InsertViewportPolicy::PreserveVisibleContent);
        if (!historySource_.isVirtual())
            setRangeAvailable(at, at, true);
        floor = at + 1;
        ++added;
    }
    if (containsCorrection)
        coalesceRevisionRows();
    resolveDisplayedMarkers();
    if (followTail && !page.isEmpty())
        scrollToEnd();
    refreshReplyPreviews();
    return added;
}

void MamChatWidget::setMediaOpener(FileSharingDeviceOpener *opener)
{
    if (mediaOpener_ == opener)
        return;
    mediaOpener_ = opener;
    for (int i = 0; i < items_.size(); ++i)
        if (rowWidget(i))
            replaceRow(i);
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
    lastMarkedIncomingTime_      = {};
    lastMarkedIncomingHistoryId_ = 0;
    unresolvedSentDisplayedIds_.clear();
    unresolvedDisplayedIds_.clear();
    cancelMessageSelection();
    historySource_.clear();
    rangeDemandAllowed_ = false;
    archiveRangeActive_ = false;
    archiveRangeWaiters_.clear();
    measuredMessageHeights_.clear();
    keys_.clear();
    sourceAliases_.clear();
    // A plain click may leave an anchor without a selected range. Reset it
    // when changing conversations, including keys reused by the next model.
    selectionAnchorKey_.clear();
    finishDragSelection();
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
    historyFillTargetPixels_ = -1;
    historyFillPaused_       = false;
    historyFillReady_        = false;
    setItemCount(0);
    timestampTimer_->stop();
}

void MamChatWidget::requestOlderMessages(const char *gesture)
{
    if (historyLoading_ || historyRequestPending_)
        return;
    if (historySource_.isVirtual()) {
        if (qstrcmp(gesture, "viewport-fill") == 0 || archiveRangeActive_)
            return;
        rangeDemandAllowed_ = true;
        const auto visible  = visibleRange();
        const int  margin   = qMax(1, qCeil(viewport()->height() / qMax(1.0, estimatedAverageMessageHeight())) / 2);
        const int  first    = qMax(0, visible.first - margin);
        const int  last     = qMin(itemCount() - 1, visible.last);
        for (int at = first; at <= last; ++at) {
            if (historySource_.isAvailable(at))
                continue;
            int end = at;
            while (end < last && !historySource_.isAvailable(end + 1))
                ++end;
            requestArchiveRange(at, end, RequestReason::Scroll, 0);
            return;
        }
        // There may still be local-only EDB rows before the server archive.
        // At the real beginning the legacy loader owns that bounded read.
        if (visible.first > 0)
            return;
    }
    if (historyExhausted_)
        return;
    if (qstrcmp(gesture, "viewport-fill") != 0) {
        historyFillPaused_       = false;
        historyFillTargetPixels_ = estimatedMessagePixels() + qMax(1, viewport()->height() / 2);
    }
    historyRequestPending_ = true;
    historyFillReady_      = false;
    ++historyRequestSerial_;
    const auto visible = visibleRange();
    qCDebug(PsiMam::historyLog).noquote()
        << "demand"
        << "gesture=" << gesture << "widget=" << this << "atEnd=" << isAtEnd() << "visible=" << visible.first
        << visible.last << "scroll=" << verticalScrollBar()->value() << "min=" << verticalScrollBar()->minimum()
        << "max=" << verticalScrollBar()->maximum() << "rows=" << messageCount() << "serial=" << historyRequestSerial_;
    emit olderMessagesRequested();
}

qint64 MamChatWidget::estimatedMessagePixels() const
{
    qint64 pixels = 0;
    const int estimatedHeight = qMax(1, qCeil(estimatedAverageMessageHeight()));
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).message.type() != MessageView::Message)
            continue;
        const QString &key = items_.at(i).key;
        if (const auto *row = rowWidget(i)) {
            const int height = qMax(1, row->height());
            measuredMessageHeights_.insert(key, qMakePair(viewport()->width(), height));
            pixels += height;
        } else {
            // Eviction must not discard learned geometry. For unseen rows the
            // visible sample is a better estimate than the old fixed 72 px.
            const auto measured = measuredMessageHeights_.value(key);
            pixels += measured.first == viewport()->width() ? measured.second : estimatedHeight;
        }
    }
    return pixels;
}

qreal MamChatWidget::estimatedAverageMessageHeight() const
{
    const auto visible = knownVisibleRange();
    qint64 pixels = 0;
    int count = 0;
    if (visible.isValid()) {
        for (int i = qMax(0, visible.first); i <= qMin(items_.size() - 1, visible.last); ++i) {
            if (items_.at(i).message.type() != MessageView::Message)
                continue;
            if (const auto *row = rowWidget(i)) {
                pixels += qMax(1, row->height());
                ++count;
            }
        }
    }
    if (count)
        return qreal(pixels) / count;
    // Before a real visible sample, use the loaded materialized rows.
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).message.type() == MessageView::Message) {
            if (const auto *row = rowWidget(i)) {
                pixels += qMax(1, row->height());
                ++count;
            }
        }
    }
    return count ? qreal(pixels) / count : 72.0;
}

int MamChatWidget::suggestedOlderHistoryPageSize() const
{
    const qreal avgHeight = qMax(1.0, estimatedAverageMessageHeight());
    const qint64 viewportPixels = qMax(1, viewport()->height());
    // Opening: fill the screen AND half a screen of real message bodies
    // above it. Later scrollback: request roughly half a screen each time.
    const bool opening = followingInitialTail_ && initialFillEligible_ && !initialFillFinished_;
    const qint64 target  = opening ? viewportPixels * 3 / 2 : historyFillTargetPixels_;
    const qint64 missing
        = target >= 0 ? qMax<qint64>(1, target - estimatedMessagePixels()) : qMax<qint64>(1, viewportPixels / 2);
    const int requiredMessages = qMax(1, int(qCeil(qreal(missing) / avgHeight)));
    constexpr int ProtocolEventsPerMessage = 3; // message + displayed + receipt.
    // At least five visible messages with up to two protocol events each.
    // Tiny remaining pixel targets must not degenerate into one-stanza pages.
    return qBound(MamHistorySource::MinRequestEvents, requiredMessages * ProtocolEventsPerMessage,
                  MamHistorySource::MaxRequestEvents);
}

void MamChatWidget::maybeContinueInitialHistoryFill()
{
    // A completed source page may contain only protocol events or duplicates.
    // Cursor progress is checked by the source; the view checks message pixels.
    initialFillEligible_ = true;
    historyFillPaused_   = false;
    historyFillReady_    = true;
    scheduleInitialHistoryFillCheck();
}

void MamChatWidget::stopHistoryFill()
{
    historyFillPaused_       = true;
    historyFillTargetPixels_ = -1;
    historyRequestPending_   = false;
}

void MamChatWidget::scheduleInitialHistoryFillCheck()
{
    const bool opening = initialFillEligible_ && !initialFillFinished_ && followingInitialTail_;
    if ((!opening && historyFillTargetPixels_ < 0) || initialFillCheckPending_ || !historyFillReady_
        || historyFillPaused_ || historyLoading_ || historyExhausted_)
        return;
    initialFillCheckPending_ = true;
    // Coalesce with LongListWidget's geometry transaction. Materialization,
    // show and completed source pages all revisit the same outstanding demand.
    QTimer::singleShot(0, this, [this] {
        initialFillCheckPending_ = false;
        const bool opening       = initialFillEligible_ && !initialFillFinished_ && followingInitialTail_;
        if ((!opening && historyFillTargetPixels_ < 0) || !historyFillReady_ || historyFillPaused_ || historyLoading_
            || historyExhausted_ || !isVisible() || viewport()->height() <= 0)
            return;
        const qint64 messagePixels = estimatedMessagePixels();
        const qint64 goal          = opening ? qint64(viewport()->height()) * 3 / 2 : historyFillTargetPixels_;
        qCDebug(PsiMam::historyLog).noquote()
            << "fill-check widget=" << this << "reason=" << (opening ? "initial" : "scroll")
            << "rows=" << messageCount() << "contentPixels=" << contentHeight() << "messagePixels=" << messagePixels
            << "viewport=" << viewport()->height() << "goal=" << goal
            << "averageMessageHeight=" << estimatedAverageMessageHeight() << "atEnd=" << isAtEnd();
        if (messagePixels >= goal) {
            if (opening)
                initialFillFinished_ = true;
            historyFillTargetPixels_ = -1;
            qCDebug(PsiMam::historyLog) << "fill-stop target-reached widget=" << this;
            if (historySource_.canSeek())
                syncSourceLayout(true);
            return;
        }
        // No page-count cap: hundreds of protocol-only events can legitimately
        // precede the next visible message. One asynchronous request at a time;
        // cancellation, exhaustion and repeating UID detection belong to source.
        requestOlderMessages("viewport-fill");
    });
}

void MamChatWidget::setHistoryLoading(bool loading)
{
    // Completion is not a scrollback request. In particular, a room archive
    // can contain many protocol-only records without filling the viewport.
    historyLoading_ = loading;
    if (loading) {
        historyRequestPending_ = false;
        historyFillReady_      = false;
    }
}

void MamChatWidget::setHistoryExhausted(bool exhausted)
{
    historyExhausted_ = exhausted;
    if (exhausted)
        historyRequestPending_ = false;
}

QWidget *MamChatWidget::createItemWidget(int logicalIndex)
{
    const int index = historySource_.knownIndex(logicalIndex);
    if (index < 0)
        return nullptr;
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
        const bool read = mv.isLocal() && !groupChat_
            && (mv.messageState().testFlag(MessageState::Displayed) || displayedTargetIndex() >= index);
        bool delivered = mv.isLocal() && mv.messageState().testFlag(MessageState::Delivered);
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
    QString    richText  = retracted ? tr("Message retracted")
        : mv.hasReplyFallback() ? mv.replyFormattedText()
                                : mv.formattedText();
    // XEP-0245 is a presentation rule, not a wire transformation. MessageView
    // has already stripped "/me " but retains isEmote(); restore the sender
    // prefix for the native widget log, just as the legacy ChatView did.
    if (!retracted && mv.isEmote()) {
        const QString actor = mv.nick().isEmpty() ? tr("Contact") : mv.nick();
        richText = QStringLiteral("*") + actor.toHtmlEscaped() + QStringLiteral(" ") + richText;
    }
    if (!retracted && !mv.formattedUserText().isEmpty())
        richText += (richText.isEmpty() ? QString() : QStringLiteral(" — ")) + mv.formattedUserText();
    // The rich-text document can paint inline media, but not arbitrary QWidget
    // attachments. Extract non-media shares, and images that are not going to
    // be downloaded by themselves, into native cards. Keep unknown shares in
    // the text so an unavailable account/item does not silently erase the
    // sender's fallback text.
    struct BinaryAttachment {
        QPointer<FileSharingItem> item;
        QUrl url;
        QString account;
        quint64 autoLimit; // see FileSharingManager::autoDownloadLimit()
    };
    QList<BinaryAttachment> files;
    // Builds this row again where a share's state decides between a card and
    // the text. Connect it queued to item signals, which can come while the
    // row is being built.
    const auto rowRebuilt = [self = QPointer<MamChatWidget>(this), key = items_.at(index).key] {
        if (!self)
            return;
        const int at = self->indexOfKey(key);
        if (at >= 0)
            self->replaceRow(at);
    };
    if (!retracted && mediaOpener_) {
        static const QRegularExpression tagRx(QStringLiteral(R"(<share\s+([^<>]*?)/>)"));
        static const QRegularExpression attrRx(QStringLiteral(R"attr((id|account|auto)\s*=\s*"([^"]*)")attr"));
        auto matches = tagRx.globalMatch(richText);
        QString stripped;
        int consumed = 0;
        while (matches.hasNext()) {
            const auto match = matches.next();
            QString id, account;
            quint64 autoLimit = 0;
            auto attributes = attrRx.globalMatch(match.captured(1));
            while (attributes.hasNext()) {
                const auto attr = attributes.next();
                if (attr.captured(1) == QLatin1String("id"))
                    id = attr.captured(2);
                else if (attr.captured(1) == QLatin1String("account"))
                    account = attr.captured(2);
                else if (attr.captured(1) == QLatin1String("auto"))
                    autoLimit = attr.captured(2).toULongLong();
            }
            FileSharingDeviceOpener *opener = id.isEmpty() ? nullptr : mediaOpener_->forAccount(account);
            FileSharingItem *item = opener ? opener->sharedItem(id) : nullptr;
            if (!item)
                continue;
            // Audio and video have their players in the text. An image is drawn
            // there once cached, or while it downloads by itself.
            const QString mime = item->mimeType();
            const bool image = mime.startsWith(QLatin1String("image/"));
            if (mime.startsWith(QLatin1String("audio/")) || mime.startsWith(QLatin1String("video/"))
                || (image && (item->isCached() || item->downloadsItself(autoLimit)))) {
                if (image && !item->isCached()) {
                    // The text draws the image once downloaded. A file that
                    // turns out too large, or fails, becomes a card instead.
                    const auto toCard = [guard = QPointer<FileSharingItem>(item), autoLimit, rowRebuilt] {
                        if (guard && !guard->isCached() && !guard->downloadsItself(autoLimit))
                            rowRebuilt();
                    };
                    connect(item, &FileSharingItem::remoteChecked, body, toCard, Qt::QueuedConnection);
                    connect(item, &FileSharingItem::downloadFinished, body, toCard, Qt::QueuedConnection);
                }
                continue;
            }
            stripped += richText.mid(consumed, match.capturedStart() - consumed);
            consumed = match.capturedEnd();
            QUrl source(QStringLiteral("share:") + id);
            if (!account.isEmpty()) {
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("account"), account);
                source.setQuery(query);
            }
            files.append({ item, source, account, autoLimit });
        }
        if (!files.isEmpty()) {
            stripped += richText.mid(consumed);
            richText = stripped;
        }
    }

    body->setRichText(richText);
    elements.insert(QStringLiteral("body"), body);
    if (!files.isEmpty()) {
        auto *attachmentGroup = new QWidget(content);
        auto *column = new QVBoxLayout(attachmentGroup);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(5);
        if (!richText.trimmed().isEmpty())
            column->addWidget(body);
        else
            body->hide();

        for (const auto &file : files) {
            if (!file.item)
                continue;
            auto *preview = new MessagePreview(palette().color(QPalette::Link), {}, attachmentGroup);
            preview->setObjectName(QStringLiteral("mamFilePreview"));
            preview->setLinkColor(palette().color(QPalette::Link));
            const QString name = file.item->displayName();
            const QString mime = file.item->mimeType();
            const QString size = file.item->fileSize()
                ? QLocale().formattedDataSize(qint64(*file.item->fileSize()))
                : tr("Unknown size");
            preview->setContent(name, tr("Type: %1\nSize: %2").arg(mime, size));
            auto *footer = new QWidget(preview);
            auto *actions = new QVBoxLayout(footer);
            actions->setContentsMargins(0, 4, 0, 0);
            actions->setSpacing(3);
            auto *controls = new QHBoxLayout;
            controls->setContentsMargins(0, 0, 0, 0);
            controls->setSpacing(5);
            auto *fileIcon = new QLabel(footer);
            QIcon icon = QIcon::fromTheme(QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension).iconName());
            if (icon.isNull())
                icon = QFileIconProvider().icon(QFileIconProvider::File);
            fileIcon->setPixmap(icon.pixmap(QSize(32, 32)));
            controls->addWidget(fileIcon);
            auto button = [this, footer, controls](QStyle::StandardPixmap iconType, const QString &tooltip) {
                auto *b = new QToolButton(footer);
                b->setIcon(style()->standardIcon(iconType));
                b->setToolTip(tooltip);
                b->setFixedSize(30, 30);
                controls->addWidget(b);
                return b;
            };
            auto *download = button(QStyle::SP_DialogSaveButton, tr("Download file"));
            auto *open = button(QStyle::SP_DialogOpenButton, tr("Open file"));
            auto *reveal = button(QStyle::SP_DirOpenIcon, tr("Show in file manager"));
            auto *copy = button(QStyle::SP_FileDialogDetailedView, tr("Copy file path"));
            // An HTTP(S) source opens in the browser without downloading here.
            const QUrl web = file.item->simpleSource();
            if (web.scheme() == QLatin1String("http") || web.scheme() == QLatin1String("https")) {
                auto *browser = button(QStyle::SP_DirLinkIcon, tr("Open in browser"));
                browser->setIcon(QIcon::fromTheme(QStringLiteral("internet-web-browser"), browser->icon()));
                connect(browser, &QToolButton::clicked, this, [web] { QDesktopServices::openUrl(web); });
            }
            auto *cancel = button(QStyle::SP_DialogCancelButton, tr("Cancel download"));
            cancel->hide();
            controls->addStretch();
            actions->addLayout(controls);
            auto *status = new QLabel(footer);
            status->setObjectName(QStringLiteral("mamFileStatus"));
            status->setTextFormat(Qt::PlainText);
            status->setTextInteractionFlags(Qt::TextSelectableByMouse);
            status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            status->setWordWrap(true);
            actions->addWidget(status);
            preview->setFooter(footer);
            // Where the message lets files download by themselves, ask the
            // server what this one is; the card shows the answer.
            if (file.item->isUrlOnly() && file.autoLimit && !file.item->isRemoteChecked()) {
                connect(file.item.data(), &FileSharingItem::remoteChecked, preview, rowRebuilt, Qt::QueuedConnection);
                file.item->checkRemote();
            }

            QString saved = file.item->property("_psiSavedFilePath").toString();
            if (!QFileInfo(saved).isFile())
                saved.clear();
            status->setText(saved);
            status->setVisible(!saved.isEmpty());
            reveal->setEnabled(!saved.isEmpty());
            copy->setEnabled(!saved.isEmpty());
            const QUrl source = file.url;
            const QString accountId = file.account;
            const QPointer<FileSharingItem> item = file.item;
            const QPointer<MamChatWidget> self(this);
            const QPointer<QLabel> statusGuard(status);
            const QPointer<QToolButton> downloadGuard(download), openGuard(open), revealGuard(reveal),
                copyGuard(copy), cancelGuard(cancel);
            const QString rowKey = items_.at(index).key;
            const auto rowChanged = [self, rowKey] {
                if (!self)
                    return;
                const int at = self->indexOfKey(rowKey);
                if (at >= 0) {
                    const int logical = self->historySource_.logicalIndex(at);
                    self->itemsChanged(logical, logical);
                }
            };
            const auto finished = [item, statusGuard, downloadGuard, openGuard, revealGuard, copyGuard,
                                   cancelGuard, rowChanged](const QString &path, const QString &error,
                                                             bool permanent) {
                if (!item)
                    return;
                if (permanent && error.isEmpty())
                    item->setProperty("_psiSavedFilePath", path);
                if (!statusGuard)
                    return; // The virtualized row was recycled during transfer.
                const bool havePath = permanent && error.isEmpty() && QFileInfo(path).isFile();
                statusGuard->setText(error.isEmpty() ? (havePath ? path : QString()) : error);
                statusGuard->setVisible(!statusGuard->text().isEmpty());
                if (downloadGuard) downloadGuard->setEnabled(true);
                if (openGuard) openGuard->setEnabled(true);
                if (revealGuard) revealGuard->setEnabled(havePath);
                if (copyGuard) copyGuard->setEnabled(havePath);
                if (cancelGuard) cancelGuard->hide();
                rowChanged();
            };
            const auto progress = [statusGuard, rowChanged](qreal fraction) {
                if (!statusGuard)
                    return;
                const QString text = fraction >= 0
                    ? QObject::tr("Downloading: %1%").arg(qRound(fraction * 100))
                    : QObject::tr("Downloading…");
                if (statusGuard->text() != text) {
                    statusGuard->setText(text);
                    statusGuard->show();
                    rowChanged();
                }
            };
            const auto started = [downloadGuard, openGuard, cancelGuard, statusGuard, rowChanged] {
                if (downloadGuard) downloadGuard->setEnabled(false);
                if (openGuard) openGuard->setEnabled(false);
                if (cancelGuard) cancelGuard->show();
                if (statusGuard) {
                    statusGuard->setText(QObject::tr("Downloading…"));
                    statusGuard->show();
                }
                rowChanged();
            };
            connect(download, &QToolButton::clicked, this, [this, accountId, source, finished, progress, started] {
                auto *opener = mediaOpener_ ? mediaOpener_->forAccount(accountId) : nullptr;
                if (opener && opener->saveAs(source, this,
                                           [finished](const QString &path, const QString &error) {
                                               finished(path, error, true);
                                           }, progress))
                    started();
            });
            connect(open, &QToolButton::clicked, this,
                    [this, item, accountId, source, finished, progress, started] {
                if (!item)
                    return;
                QString existing = item->property("_psiSavedFilePath").toString();
                if (!QFileInfo(existing).isFile() && item->isCached())
                    existing = item->fileName();
                if (QFileInfo(existing).isFile()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(existing));
                    return;
                }
                auto *opener = mediaOpener_ ? mediaOpener_->forAccount(accountId) : nullptr;
                if (!opener)
                    return;
                auto directory = std::make_shared<QTemporaryDir>();
                if (!directory->isValid()) {
                    finished({}, tr("Could not create a temporary directory"), false);
                    return;
                }
                const QString fileName = QFileInfo(item->displayName()).fileName();
                const QString path = directory->filePath(fileName.isEmpty() ? QStringLiteral("attachment") : fileName);
                if (opener->saveTo(source, path,
                    [finished, directory](const QString &saved, const QString &error) {
                        finished(saved, error, false);
                        if (error.isEmpty() && QFileInfo(saved).isFile())
                            QDesktopServices::openUrl(QUrl::fromLocalFile(saved));
                    }, progress)) {
                    openedTemporaryFiles_.append(directory);
                    started();
                }
            });
            connect(cancel, &QToolButton::clicked, this, [this, source, accountId] {
                if (auto *opener = mediaOpener_ ? mediaOpener_->forAccount(accountId) : nullptr)
                    opener->cancelSave(source);
            });
            connect(reveal, &QToolButton::clicked, this, [item] {
                if (!item)
                    return;
                const QString path = item->property("_psiSavedFilePath").toString();
                if (QFileInfo(path).isFile())
                    FileUtil::openFolder(path);
            });
            connect(copy, &QToolButton::clicked, this, [item] {
                if (!item)
                    return;
                const QString path = item->property("_psiSavedFilePath").toString();
                if (QFileInfo(path).isFile())
                    QApplication::clipboard()->setText(path);
            });
            column->addWidget(preview);
            preview->installEventFilter(this);
            for (auto *child : preview->findChildren<QWidget *>())
                child->installEventFilter(this);
        }
        elements[QStringLiteral("body")] = attachmentGroup;
    }
    body->installEventFilter(this);
    body->viewport()->installEventFilter(this);

    // As in Bubble, each actor sends its entire current set, not an add/remove
    // delta. Chips aggregate actors and toggling a chip updates our own set.
    const QString              target = reactionKey(items_.at(index));
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

    if (!retracted && mv.reply().isValid()) {
        auto *group = new QWidget(content);
        auto *column = new QVBoxLayout(group);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);
        auto *preview = new MessagePreview(quoteColor_, quoteColor_, group);
        preview->setLinkColor(palette().color(QPalette::Link));
        preview->setObjectName(QStringLiteral("mamReplyPreview"));
        QString author = mv.reply().author;
        QString text = mv.reply().text;
        const bool resolved = resolveReply(mv.reply(), author, text);
        preview->setProperty("replyResolved", resolved);
        preview->setContent(author.isEmpty() ? tr("Quoted message") : author,
                            text.isEmpty() ? tr("Referenced message is not available locally") : text);
        connect(preview, &MessagePreview::heightChanged, this, [this, key = items_.at(index).key] {
            const int at = indexOfKey(key);
            if (at >= 0) {
                const int logical = historySource_.logicalIndex(at);
                itemsChanged(logical, logical);
            }
        });
        column->addWidget(preview);
        preview->installEventFilter(this);
        for (auto *child : preview->findChildren<QWidget *>())
            child->installEventFilter(this);
        column->addWidget(elements.value(QStringLiteral("body")));
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

    const auto range = knownVisibleRange();
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
        auto *row = rowWidget(i);
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
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        setHoveredRow(nullptr);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange)
        schedulePaletteRefresh();
    Mattermost::LongListWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        measuredMessageHeights_.clear();
        for (int index : materializedRows()) {
            auto *row = rowWidget(index);
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
            for (int at : materializedRows()) {
                const int logical = historySource_.logicalIndex(at);
                itemsChanged(logical, logical);
            }
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
    for (int index : materializedRows())
        if (auto *row = rowWidget(index)) {
            if (auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
                updateTimestampAppearance(time);
            if (auto *preview = row->findChild<MessagePreview *>(QStringLiteral("mamReplyPreview")))
                preview->setLinkColor(palette().color(QPalette::Link));
            for (auto *fileCard : row->findChildren<MessagePreview *>(QStringLiteral("mamFilePreview"))) {
                fileCard->setColors(palette().color(QPalette::Link));
                fileCard->setLinkColor(palette().color(QPalette::Link));
            }
        }
}

void MamChatWidget::setTheme(const NativeChatTheme &theme)
{
    measuredMessageHeights_.clear();
    theme_ = theme;
    for (int index : materializedRows())
        replaceRow(index);
    updateTimestamps();
}

void MamChatWidget::setNicknameColorResolver(std::function<QColor(const MessageView &)> resolver)
{
    nicknameColorResolver_ = std::move(resolver);
    refreshNicknameColors();
}

void MamChatWidget::refreshNicknameColors()
{
    for (int index : materializedRows()) {
        if (auto *row = rowWidget(index))
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
    for (int i : materializedRows()) {
        if (auto *row = rowWidget(i))
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
    finishDragSelection();
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
        blocks.append(copiedMessage(row));
    }
    if (!blocks.isEmpty())
        QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n\n")));
}

QString MamChatWidget::plainMessage(const Row &row) const
{
    const auto   &message = row.message;
    QTextDocument document;
    const bool    retracted = isRetracted(row);
    QString       rich      = retracted         ? tr("Message retracted")
                   : message.hasReplyFallback() ? message.replyFormattedText()
                                                : message.formattedText();
    if (!retracted && message.isEmote()) {
        const QString actor = message.nick().isEmpty() ? tr("Contact") : message.nick();
        rich                = QStringLiteral("*") + actor.toHtmlEscaped() + QStringLiteral(" ") + rich;
    }
    if (!retracted && !message.formattedUserText().isEmpty())
        rich += (rich.isEmpty() ? QString() : QStringLiteral(" — ")) + message.formattedUserText();
    PsiRichText::setText(&document, rich);
    return PsiRichText::convertToPlainText(&document).trimmed();
}

QString MamChatWidget::copiedMessage(const Row &row) const
{
    const auto   &message = row.message;
    const QString nick    = message.nick().isEmpty() ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
    const QString timestamp = message.dateTime().isValid()
        ? message.dateTime().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QString();
    return QStringLiteral("%1\t[%2]\n%3").arg(nick, timestamp, plainMessage(row));
}

bool MamChatWidget::resolveReply(const ChatReply &reply, QString &author, QString &text) const
{
    // A plain-text quote has no id: never match it to a row, whose own
    // reaction target may be just as empty in a room without stanza-ids.
    if (!reply.hasReference())
        return false;
    const Row *match = nullptr;
    for (const auto &candidate : items_) {
        const QString sender = candidate.message.userId();
        const bool sameSender = reply.to.isEmpty() || sender == reply.to
            || (!groupChat_ && !reply.to.contains(QLatin1Char('/')) && sender.section(QLatin1Char('/'), 0, 0) == reply.to);
        if (candidate.message.type() != MessageView::Message || !sameSender)
            continue;
        if (reactionTarget(candidate.message) == reply.id || (!groupChat_ && candidate.stanzaIds.contains(reply.id))) {
            if (match)
                return false; // Reused IDs are ambiguous; keep the explicit fallback.
            match = &candidate;
        }
    }
    if (!match)
        return false;
    author = match->message.nick();
    text = plainMessage(*match);
    return true;
}

void MamChatWidget::refreshReplyPreviews(bool force)
{
    for (const int index : materializedRows()) {
        auto *row = rowWidget(index);
        auto *preview = row ? row->findChild<MessagePreview *>(QStringLiteral("mamReplyPreview")) : nullptr;
        if (!preview || (!force && preview->property("replyResolved").toBool()))
            continue;
        QString author, text;
        if (resolveReply(items_.at(index).message.reply(), author, text)) {
            preview->setProperty("replyResolved", true);
            preview->setContent(author, text);
        }
    }
}

// A hit is an actual rendered image fragment, not simply a nearby text
// cursor (QTextEdit can return the previous character at an image boundary).
static std::optional<QTextImageFormat> imageAt(ChatMessageText *body, const QPoint &point)
{
    for (auto block = body->document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isImageFormat())
                continue;
            const QTextImageFormat format = fragment.charFormat().toImageFormat();
            QTextCursor cursor(body->document());
            cursor.setPosition(fragment.position());
            const QPoint origin = body->cursorRect(cursor).topLeft();
            const QSize size(qRound(format.width()), qRound(format.height()));
            if (size.isValid() && QRect(origin, size).contains(point))
                return format;
        }
    }
    return std::nullopt;
}

// Image objects from SIMS have share: IDs instead of a public URL. Resolve
// their owning account using the original markup; no cross-account ID lookup.
static FileSharingItem *sharedImageItem(const QTextImageFormat &format,
                                        const QString &messageHtml,
                                        FileSharingDeviceOpener *mediaOpener)
{
    const QUrl source(format.name());
    if (source.scheme() != QLatin1String("share") || !mediaOpener)
        return nullptr;

    const QString id = source.path();
    static const QRegularExpression tagRx(QStringLiteral(R"(<share\s+([^<>]*?)/>)"));
    static const QRegularExpression attrRx(QStringLiteral(R"attr((id|account)\s*=\s*"([^"]*)")attr"));
    auto tags = tagRx.globalMatch(messageHtml);
    while (tags.hasNext()) {
        const auto tag = tags.next();
        QString tagId, account;
        auto attrs = attrRx.globalMatch(tag.captured(1));
        while (attrs.hasNext()) {
            const auto attr = attrs.next();
            if (attr.captured(1) == QLatin1String("id"))
                tagId = attr.captured(2);
            else if (attr.captured(1) == QLatin1String("account"))
                account = attr.captured(2);
        }
        if (tagId == id) {
            auto *opener = mediaOpener->forAccount(account);
            auto *item = opener ? opener->sharedItem(id) : nullptr;
            if (item && item->mimeType().startsWith(QLatin1String("image/")))
                return item;
        }
    }
    return nullptr;
}

static QString httpImageLink(const QTextImageFormat &format, const QString &messageHtml,
                             FileSharingDeviceOpener *mediaOpener)
{
    QUrl url(format.name());
    if (url.scheme() == QLatin1String("share")) {
        auto *item = sharedImageItem(format, messageHtml, mediaOpener);
        url = item ? item->simpleSource() : QUrl();
    }
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return {};
    return url.toString(QUrl::FullyEncoded);
}

static QImage fullImage(const QTextImageFormat &format, const QString &messageHtml,
                        ChatMessageText *body, FileSharingDeviceOpener *mediaOpener)
{
    if (auto *item = sharedImageItem(format, messageHtml, mediaOpener)) {
        // Use the cached original when available. The chat's inline resource
        // is commonly just a downscaled 640x480 preview.
        const QString path = item->fileName();
        if (!path.isEmpty() && QFileInfo(path).isFile()) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QImage original = reader.read();
            if (!original.isNull())
                return original;
        }
    }
    const QVariant value = body->document()->resource(QTextDocument::ImageResource, QUrl(format.name()));
    QImage preview = value.value<QImage>();
    if (preview.isNull())
        preview = value.value<QPixmap>().toImage();
    return preview;
}

static void openImageViewer(const QImage &image)
{
    if (image.isNull())
        return;
    auto *viewer = new MamImageViewer(image);
    viewer->show();
}

QMenu *MamChatWidget::createMessageContextMenu(const QString &key, QWidget *target, const QPoint &globalPosition)
{
    const int at = indexOfKey(key);
    if (at < 0 || items_.at(at).message.type() != MessageView::Message)
        return nullptr;
    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("mamMessageContextMenu"));
    menu->setToolTipsVisible(true);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto *row = rowWidget(at);
    QPointer<ChatMessageText> body = row ? row->findChild<ChatMessageText *>() : nullptr;
    QPointer<QTextEdit> selectedTextView = body.data();
    QPointer<QLabel> selectedLabel = qobject_cast<QLabel *>(target);
    for (auto *child = target; child && child != row; child = child->parentWidget())
        if (auto *text = qobject_cast<QTextEdit *>(child)) {
            selectedTextView = text;
            break;
        }
    auto *copy = menu->addAction(tr("Copy"));
    copy->setObjectName(QStringLiteral("mamCopyMessage"));
    connect(copy, &QAction::triggered, this, [this, key, selectedTextView, selectedLabel] {
        if (selectedLabel && selectedLabel->hasSelectedText())
            QApplication::clipboard()->setText(selectedLabel->selectedText());
        else if (selectedTextView && selectedTextView->textCursor().hasSelection())
            selectedTextView->copy();
        else {
            const int at = indexOfKey(key);
            if (at >= 0)
                QApplication::clipboard()->setText(copiedMessage(items_.at(at)));
        }
    });
    auto *quote = menu->addAction(tr("Quote"));
    quote->setObjectName(QStringLiteral("mamQuoteMessage"));
    // Group chats without stanza-ids still allow a plain-text quote (no reply
    // reference); see applyMessageReply().
    quote->setEnabled((groupChat_ || !reactionTarget(items_.at(at).message).isEmpty())
                      && !isRetracted(items_.at(at)));
    if (!quote->isEnabled())
        quote->setToolTip(tr("The message has no reply identifier assigned by its sender or room."));
    connect(quote, &QAction::triggered, this, [this, key] {
        const int at = indexOfKey(key);
        if (at < 0 || isRetracted(items_.at(at)))
            return;
        const auto &message = items_.at(at).message;
        emit replyRequested({reactionTarget(message), message.userId(), message.nick(), plainMessage(items_.at(at))});
    });
    auto *select = menu->addAction(tr("Select message"));
    select->setObjectName(QStringLiteral("mamSelectMessage"));
    select->setCheckable(true);
    select->setChecked(selectedKeys_.contains(key));
    connect(select, &QAction::triggered, this, [this, key](bool selected) {
        const int at = indexOfKey(key);
        if (at >= 0)
            setMessageSelected(at, selected);
    });
    auto *forward = menu->addAction(tr("Forward"));
    forward->setObjectName(QStringLiteral("mamForwardMessage"));
    forward->setEnabled(!isRetracted(items_.at(at)));
    connect(forward, &QAction::triggered, this, [this, key] {
        const int at = indexOfKey(key);
        if (at >= 0 && !isRetracted(items_.at(at))) {
            const auto &message = items_.at(at).message;
            emit forwardMessageRequested(message.messageId(), message.userId(), plainMessage(items_.at(at)));
        }
    });
    if (body && (target == body || target == body->viewport())) {
        const QPoint point = body->viewport()->mapFromGlobal(globalPosition);
        const auto format = imageAt(body, point);
        if (format) {
            const auto &message = items_.at(at).message;
            const QString html = message.hasReplyFallback() ? message.replyFormattedText() : message.formattedText();
            const QImage image = fullImage(*format, html, body, mediaOpener_);
            const QString link = httpImageLink(*format, html, mediaOpener_);
            if (!image.isNull() || !link.isEmpty()) {
                menu->addSeparator();
                if (!image.isNull()) {
                    auto *copyImage = menu->addAction(tr("Copy image"));
                    copyImage->setObjectName(QStringLiteral("mamCopyImage"));
                    connect(copyImage, &QAction::triggered, this,
                            [image] { QApplication::clipboard()->setImage(image); });
                    auto *viewImage = menu->addAction(tr("View image"));
                    viewImage->setObjectName(QStringLiteral("mamViewImage"));
                    connect(viewImage, &QAction::triggered, this,
                            [image] { openImageViewer(image); });
                }
                if (!link.isEmpty()) {
                    auto *copyImageLink = menu->addAction(tr("Copy image link"));
                    copyImageLink->setObjectName(QStringLiteral("mamCopyImageLink"));
                    connect(copyImageLink, &QAction::triggered, this,
                            [link] { QApplication::clipboard()->setText(link); });
                }
            }
        } else {
            const QString link = body->anchorAt(point);
            if (!link.isEmpty()) {
                menu->addSeparator();
                connect(menu->addAction(tr("Open link")), &QAction::triggered, this,
                        [link] { URLObject::getInstance()->popupAction(link); });
                connect(menu->addAction(tr("Copy link address")), &QAction::triggered, this,
                        [link] { QApplication::clipboard()->setText(link); });
            }
        }
    }
    return menu;
}

void MamChatWidget::setInformationalColor(const QColor &color)
{
    informationalColor_ = color;
    refreshSelection();
}

void MamChatWidget::setQuoteColor(const QColor &color)
{
    quoteColor_ = color;
    for (int index : materializedRows())
        if (auto *row = rowWidget(index))
            if (auto *preview = row->findChild<MessagePreview *>(QStringLiteral("mamReplyPreview")))
                preview->setColors(color, color);
}

void MamChatWidget::updateRowSelection(QFrame *row, bool selected)
{
    const bool selecting = !selectedKeys_.isEmpty();
    if (auto *check = row->findChild<QCheckBox *>(QStringLiteral("mamMessageSelection"))) {
        const QSignalBlocker blocker(check);
        check->setChecked(selected);
        check->setVisible(selecting);
    }
    updateRowPalette(row, selected);
    if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"))) {
        author->setTextInteractionFlags(selecting ? Qt::NoTextInteraction : Qt::TextSelectableByMouse);
        if (selecting)
            author->setSelection(0, 0);
    }
    if (auto *body = row->findChild<ChatMessageText *>())
        body->setWholeMessageSelectionMode(selecting);
}

void MamChatWidget::setHoveredRow(QFrame *row)
{
    auto *previous = qobject_cast<QFrame *>(hoverHighlightOverrideWidget());
    if (previous == row)
        return;
    setHoverHighlightOverride(row);
    for (auto *changed : { previous, row })
        if (changed)
            updateRowPalette(changed, selectedKeys_.contains(changed->property("_psiMamRowKey").toString()));
}

void MamChatWidget::updateRowPalette(QFrame *row, bool selected)
{
    const QColor base    = viewport()->palette().color(QPalette::Base);
    const QColor accent  = viewport()->palette().color(QPalette::Highlight);
    const QColor surface = selected
        ? QColor((base.red() * 3 + accent.red()) / 4, (base.green() * 3 + accent.green()) / 4,
                 (base.blue() * 3 + accent.blue()) / 4)
        : (hoverHighlightOverrideWidget() == row ? hoverHighlightSurfaceColor() : base);
    QPalette     palette = viewport()->palette();
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::Base, surface); // QTextDocument's surface, too.
    row->setPalette(palette);
    row->setAutoFillBackground(true);
    if (auto *body = row->findChild<ChatMessageText *>()) {
        QPalette textPalette = palette;
        if (row->property("_psiMamInformational").toBool())
            textPalette.setColor(QPalette::Text,
                                 informationalColor_.isValid() ? informationalColor_ : palette.color(QPalette::Link));
        body->setPalette(textPalette);
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
            auto *row = qobject_cast<QFrame *>(rowWidget(i));
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
    rangeDemandAllowed_ = false;
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
    if (archiveRangeActive_ && archiveRangePaused_) {
        archiveRangePaused_ = false;
        emit archiveRangeRequested(activeArchiveRange_);
    }
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
    setHoveredRow(nullptr);
    timestampTimer_->stop();
    rangeDemandAllowed_ = false;
    Mattermost::LongListWidget::hideEvent(event);
}

bool MamChatWidget::eventFilter(QObject *target, QEvent *event)
{
    if (target == viewport() && selectingByDrag_ && QWidget::mouseGrabber() == viewport()) {
        // Whole-message drag selection owns the pointer (see updateDragSelection).
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseMove) {
            if (event->spontaneous() && !mouse->buttons().testFlag(Qt::LeftButton)
                && !QGuiApplication::mouseButtons().testFlag(Qt::LeftButton))
                finishDragSelection(); // The release was lost; do not keep selecting.
            else
                updateDragSelection(mouse->pos());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton) {
            finishDragSelection();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
            return true;
    }
    if (event->type() == QEvent::ContextMenu) {
        if (auto *row = eventRow(target)) {
            auto *context = static_cast<QContextMenuEvent *>(event);
            for (auto *child = qobject_cast<QWidget *>(target); groupChat_ && child && child != row;
                 child = child->parentWidget()) {
                if (child->objectName() == QLatin1String("mamAuthor") || child->objectName() == QLatin1String("mamAvatar")) {
                    const int at = indexOfKey(row->property("_psiMamRowKey").toString());
                    if (at >= 0)
                        emit contactMenuRequested(items_.at(at).message.nick(), context->globalPos());
                    context->accept();
                    return true;
                }
            }
            if (auto *menu = createMessageContextMenu(row->property("_psiMamRowKey").toString(),
                                                     qobject_cast<QWidget *>(target), context->globalPos())) {
                hideMessageActions();
                menu->popup(context->globalPos());
                context->accept();
                return true;
            }
        }
    }
    if (event->type() == QEvent::MouseButtonPress) {
        pressedImageBody_.clear();
        auto *widget = qobject_cast<QWidget *>(target);
        auto *body = widget ? qobject_cast<ChatMessageText *>(widget->parentWidget()) : nullptr;
        if (body && widget == body->viewport() && selectedKeys_.isEmpty()) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier
                && imageAt(body, mouse->pos())) {
                pressedImageBody_ = body;
                pressedImagePoint_ = mouse->pos();
            }
        }
    }
    // A Leave from a row can be just a handoff to the floating actions. The
    // viewport encloses the complete hover session, including that overlay.
    if (target == viewport() && (event->type() == QEvent::Enter || event->type() == QEvent::Leave))
        setHoveredRow(nullptr);
    if (target == verticalScrollBar()) {
        // A scrollbar at zero cannot emit valueChanged; Qt may even omit
        // actionTriggered for an outward user action that clamps to zero.
        // Observe actual input, do not intercept it or react to geometry.
        bool upward = false;
        if (event->type() == QEvent::Wheel) {
            auto *wheel = static_cast<QWheelEvent *>(event);
            upward = (wheel->pixelDelta().y() != 0 ? wheel->pixelDelta().y()
                                                    : wheel->angleDelta().y()) > 0;
        } else if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            upward = key == Qt::Key_Up || key == Qt::Key_PageUp || key == Qt::Key_Home;
        }
        if (upward) {
            const quint64 serial = historyRequestSerial_;
            const auto visible = visibleRange();
            // Capture the boundary at the moment of user input. Deferred
            // showEvent/layout updates may move the scrollbar before we handle
            // the gesture, but must not erase that initial user demand.
            const bool atOldest = verticalScrollBar()->value() == verticalScrollBar()->minimum()
                || (visible.isValid() && visible.first <= 1);
            QTimer::singleShot(0, this, [this, serial, atOldest] {
                if (historyRequestSerial_ == serial && atOldest)
                    requestOlderMessages("scrollbar-input");
            });
        }
    }
    auto      *widget  = qobject_cast<QWidget *>(target);
    const bool hover   = event->type() == QEvent::Enter || event->type() == QEvent::Leave;
    const bool inQuick = hover && widget && (widget == quickReactions_ || quickReactions_->isAncestorOf(widget));
    const bool inActions = hover && widget && (widget == messageActions_ || messageActions_->isAncestorOf(widget));
    if (event->type() == QEvent::Enter) {
        // The toolbar and all of its children share the row's hover session,
        // including the react button and the expanding quick-reaction slot.
        if (inActions) {
            actionHideTimer_->stop();
            if (actionsFade_->state() == QAbstractAnimation::Running && actionsFade_->endValue().toReal() <= 0.0) {
                actionsFade_->stop();
                actionsFade_->setStartValue(actionsOpacity_->opacity());
                actionsFade_->setEndValue(1.0);
                actionsFade_->start();
            }
        }
        if (target == chooseReaction_)
            setQuickReactionsExpanded(true);
        else if (inQuick)
            quickHideTimer_->stop();
        else if (!inActions) {
            if (auto *row = eventRow(target)) {
                if (isHoverHighlightEnabled())
                    setHoveredRow(row);
                showMessageActions(row);
            }
        }
    } else if (event->type() == QEvent::Leave) {
        if (target == chooseReaction_ || inQuick)
            quickHideTimer_->start();
        // Leaving a text viewport/label for row padding is still inside the
        // message. Only the enclosing row or toolbar can start dismissal.
        if (target == messageActions_ || target == eventRow(target))
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
            // Excerpts have independent text selection. Do not turn dragging
            // inside a quote into whole-message selection in the outer list.
            for (auto *child = qobject_cast<QWidget *>(target); child && child != row; child = child->parentWidget())
                if (qobject_cast<MessagePreview *>(child))
                    return Mattermost::LongListWidget::eventFilter(target, event);
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

                    // A real (spontaneous) move with no button held means the
                    // release went elsewhere; end the gesture instead of
                    // selecting on hover.
                    if (event->spontaneous() && !mouse->buttons().testFlag(Qt::LeftButton)
                        && !QGuiApplication::mouseButtons().testFlag(Qt::LeftButton)) {
                        const bool wasSelecting = selectingByDrag_;
                        finishDragSelection();
                        if (wasSelecting)
                            return true;
                        return Mattermost::LongListWidget::eventFilter(target, event);
                    }
                    auto        *source = qobject_cast<QWidget *>(target);
                    const QPoint where  = source->mapTo(viewport(), mouse->pos());
                    if (updateDragSelection(where))
                        return true;
                } else if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton) {
                    const bool consumed = selectingByDrag_ || swallowMouseRelease_;
                    finishDragSelection();
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
    // Open images on a genuine click, not when the user drags to select text.
    // Keep this after whole-message selection handling so its press/release
    // state has already been cleared.
    if (event->type() == QEvent::MouseButtonRelease && pressedImageBody_) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        QPointer<ChatMessageText> body = pressedImageBody_;
        pressedImageBody_.clear();
        if (body && target == body->viewport() && mouse->button() == Qt::LeftButton
            && mouse->modifiers() == Qt::NoModifier && selectedKeys_.isEmpty()
            && (pressedImagePoint_ - mouse->pos()).manhattanLength() <= QApplication::startDragDistance()) {
            if (const auto format = imageAt(body, mouse->pos())) {
                if (auto *row = eventRow(target)) {
                    const int at = indexOfKey(row->property("_psiMamRowKey").toString());
                    if (at >= 0) {
                        const auto &message = items_.at(at).message;
                        const QString html = message.hasReplyFallback() ? message.replyFormattedText()
                                                                         : message.formattedText();
                        const QImage image = fullImage(*format, html, body, mediaOpener_);
                        if (!image.isNull()) {
                            openImageViewer(image);
                            mouse->accept();
                            return true;
                        }
                    }
                }
            }
        }
    }
    return Mattermost::LongListWidget::eventFilter(target, event);
}
