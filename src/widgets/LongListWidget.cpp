#include "LongListWidget.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

#include <QEvent>
#include <QLayout>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWheelEvent>

namespace Mattermost {

namespace {

constexpr int HoverHighlightAlpha = 24;

bool sameRange(const LongListWidget::Range& lhs, const LongListWidget::Range& rhs)
{
    return lhs.first == rhs.first && lhs.last == rhs.last;
}

int boundedHeight(int height)
{
    return std::max(1, height);
}

QBitArray insertedBits(const QBitArray& source, int first, int count)
{
    const int oldSize = static_cast<int>(source.size());
    first = std::max(0, std::min(first, oldSize));
    count = std::max(0, count);

    QBitArray result(oldSize + count, false);
    for (int index = 0; index < first; ++index) {
        result.setBit(index, source.testBit(index));
    }
    for (int index = first; index < oldSize; ++index) {
        result.setBit(index + count, source.testBit(index));
    }
    return result;
}

QBitArray removedBits(const QBitArray& source, int first, int count)
{
    const int oldSize = static_cast<int>(source.size());
    first = std::max(0, std::min(first, oldSize));
    count = std::max(0, std::min(count, oldSize - first));

    QBitArray result(oldSize - count, false);
    for (int index = 0; index < first; ++index) {
        result.setBit(index, source.testBit(index));
    }
    for (int index = first + count; index < oldSize; ++index) {
        result.setBit(index - count, source.testBit(index));
    }
    return result;
}

} // namespace

void LongListWidget::HeightIndex::reset(int count, int height)
{
    count = std::max(0, count);
    height = boundedHeight(height);
    values.fill(height, count);
    rebuildFenwick();
}

void LongListWidget::HeightIndex::resize(int count, int height)
{
    count = std::max(0, count);
    const QVector<int> previous = values;
    reset(count, height);
    const int preserved = std::min(static_cast<int>(previous.size()), count);
    for (int index = 0; index < preserved; ++index) {
        setValue(index, previous.at(index));
    }
}

void LongListWidget::HeightIndex::insert(int first, int count, int height)
{
    first = std::max(0, std::min(first, static_cast<int>(values.size())));
    count = std::max(0, count);
    height = boundedHeight(height);
    for (int offset = 0; offset < count; ++offset) {
        values.insert(first + offset, height);
    }
    rebuildFenwick();
}

void LongListWidget::HeightIndex::remove(int first, int count)
{
    first = std::max(0, std::min(first, static_cast<int>(values.size())));
    count = std::max(0, std::min(count, static_cast<int>(values.size()) - first));
    for (int offset = 0; offset < count; ++offset) {
        values.removeAt(first);
    }
    rebuildFenwick();
}

int LongListWidget::HeightIndex::value(int index) const
{
    return index >= 0 && index < values.size() ? values.at(index) : 0;
}

void LongListWidget::HeightIndex::setValue(int index, int value)
{
    if (index < 0 || index >= values.size()) {
        return;
    }
    value = boundedHeight(value);
    const qint64 delta = static_cast<qint64>(value) - values.at(index);
    if (delta == 0) {
        return;
    }
    values[index] = value;
    add(index, delta);
}

void LongListWidget::HeightIndex::add(int index, qint64 delta)
{
    for (int pos = index + 1; pos < fenwick.size(); pos += pos & -pos) {
        fenwick[pos] += delta;
    }
}

void LongListWidget::HeightIndex::rebuildFenwick()
{
    fenwick.fill(0, values.size() + 1);
    for (int index = 0; index < values.size(); ++index) {
        add(index, values.at(index));
    }
}

qint64 LongListWidget::HeightIndex::prefixHeight(int count) const
{
    count = std::max(0, std::min(count, static_cast<int>(values.size())));
    qint64 result = 0;
    for (int pos = count; pos > 0; pos -= pos & -pos) {
        result += fenwick.at(pos);
    }
    return result;
}

int LongListWidget::HeightIndex::indexAtPixel(qint64 pixel) const
{
    if (values.isEmpty()) {
        return -1;
    }

    const qint64 total = totalHeight();
    if (total <= 0) {
        return 0;
    }
    pixel = std::max<qint64>(0, std::min(pixel, total - 1));

    int index = 0;
    qint64 accumulated = 0;
    int bit = 1;
    while ((bit << 1) < fenwick.size()) {
        bit <<= 1;
    }

    for (; bit != 0; bit >>= 1) {
        const int next = index + bit;
        if (next < fenwick.size() && accumulated + fenwick.at(next) <= pixel) {
            index = next;
            accumulated += fenwick.at(next);
        }
    }

    return std::min(index, static_cast<int>(values.size()) - 1);
}

LongListWidget::LongListWidget(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setAutoFillBackground(true);

    syncTimer.setSingleShot(true);
    connect(&syncTimer, &QTimer::timeout, this, &LongListWidget::synchronize);

    geometryTimer.setSingleShot(true);
    connect(&geometryTimer, &QTimer::timeout, this,
            [this] { commitGeometry(false); });

    seekTimer.setSingleShot(true);
    seekTimer.setInterval(seekDebounceInterval);
    connect(&seekTimer, &QTimer::timeout, this, &LongListWidget::activateSeekTarget);

    viewportLockTimer.setSingleShot(true);
    connect(&viewportLockTimer, &QTimer::timeout, this, [this] {
        releaseViewportLock(true);
    });

    scrollAnimation.setStartValue(0.0);
    scrollAnimation.setEndValue(1.0);
    scrollAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&scrollAnimation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant& value) {
        QScrollBar* bar = verticalScrollBar();
        const qreal progress = value.toReal();
        const int target = scrollAnimationStartValue
            + qRound((bar->maximum() - scrollAnimationStartValue) * progress);

        scrollAnimationApplying = true;
        internalScrollChange = true;
        bar->setValue(target);
        internalScrollChange = false;
        scrollAnimationApplying = false;

        layoutMaterialized();
        scheduleViewportSyncIfNeeded(RequestReason::Scroll);
    });
    connect(&scrollAnimation, &QVariantAnimation::finished, this, [this] {
        QScrollBar* bar = verticalScrollBar();
        internalScrollChange = true;
        bar->setValue(bar->maximum());
        internalScrollChange = false;
        layoutMaterialized();
        scheduleSync(RequestReason::EnsureVisible);
        emitRangeChanges(VisibilityChangeReason::ProgrammaticScroll);
    });

    QScrollBar* bar = verticalScrollBar();
    connect(bar, &QScrollBar::valueChanged, this, [this] { onScrollValueChanged(); });
    connect(bar, &QScrollBar::sliderMoved, this, &LongListWidget::onSliderMoved);
    connect(bar, &QScrollBar::actionTriggered, this, [this, bar](int action) {
        userScrollActionPending = true;
        noteUserScrollStarted();
        QTimer::singleShot(0, this, [this] { userScrollActionPending = false; });
        // An absolute groove click updates sliderPosition and, with tracking
        // enabled, emits SliderMove before QScrollBar marks its handle down. It
        // therefore has no sliderMoved() signal even though it is the same kind
        // of random logical jump as dragging the thumb. Start that one-shot seek
        // immediately; otherwise a sparse timeline is treated as ordinary scroll
        // and can depend on unrelated model/geometry changes to continue loading.
        if (action == QAbstractSlider::SliderMove && !bar->isSliderDown()) {
            onSliderMoved(bar->sliderPosition());
            seekTimer.stop();
            activateSeekTarget();
        }

    });
    connect(bar, &QScrollBar::sliderReleased, this, [this, bar] {
        noteUserScrollStarted();
        onSliderMoved(bar->value());
        seekTimer.stop();
        activateSeekTarget();
    });
}

LongListWidget::~LongListWidget()
{
    const auto widgets = materialized;
    materialized.clear();
    widgetIndexes.clear();
    for (auto it = widgets.cbegin(); it != widgets.cend(); ++it) {
        destroyItemWidget(it.key(), it.value());
    }
}

void LongListWidget::setItemCount(int count)
{
    count = std::max(0, count);
    if (count == logicalCount) {
        return;
    }

    // Growing the logical tail changes maximumContentOffset immediately, before
    // commitGeometry() can capture its anchor. Remember an existing sticky-end
    // state while the old geometry is still authoritative and restore it after
    // resizing the height index. A semantic viewport lock has stronger intent
    // than sticky-bottom and is restored by commitGeometry() instead.
    const bool preserveBottom = logicalCount > 0 && !hasViewportLock() && isAtEnd();

    const int oldCount = logicalCount;
    logicalCount = count;
    lastSynchronizedViewportRange = {};
    heights.resize(count, defaultHeight);
    measured.resize(count);
    available.resize(count);
    pendingRequest.resize(count);

    for (int index = oldCount; index < count; ++index) {
        heights.setValue(index, estimatedItemHeight(index));
    }

    const QVector<int> current = materializedIndices();
    for (int index : current) {
        if (index >= count) {
            QWidget* widget = materialized.take(index);
            if (widget) {
                widgetIndexes.remove(widget);
                widget->removeEventFilter(this);
                destroyItemWidget(index, widget);
            }
        }
    }

    if (seekTarget >= count) {
        clearSeek();
    }
    if (viewportLock.index >= count) {
        releaseViewportLock(true);
    }

    commitGeometry(true);
    if (preserveBottom && logicalCount > 0) {
        scrollToEnd();
    } else {
        scheduleSync(RequestReason::Initial);
    }
}

void LongListWidget::insertItems(int first,
                                 int count,
                                 InsertViewportPolicy viewportPolicy)
{
    first = std::max(0, std::min(first, logicalCount));
    count = std::max(0, count);
    if (count == 0) {
        return;
    }

    ViewAnchor anchor = captureAnchor();
    const qint64 oldOffset = contentOffset();
    if (viewportPolicy == InsertViewportPolicy::PreserveVisibleContent
        && anchor.kind == ViewAnchor::Bottom && logicalCount > 0) {
        const int oldTopIndex = heights.indexAtPixel(oldOffset);
        if (oldTopIndex >= 0) {
            anchor.kind = ViewAnchor::Item;
            anchor.index = oldTopIndex;
            anchor.offsetInsideItem =
                oldOffset - heights.prefixHeight(oldTopIndex);
        }
    }
    if (anchor.kind == ViewAnchor::Item && anchor.index >= first) {
        anchor.index += count;
    }
    if (viewportLock.index >= first) {
        viewportLock.index += count;
    }

    logicalCount += count;
    lastSynchronizedViewportRange = {};
    heights.insert(first, count, defaultHeight);
    measured = insertedBits(measured, first, count);
    available = insertedBits(available, first, count);

    // A structural shift changes the coordinate system of any outstanding
    // logical-range request. Drop view-side suppression; the service layer still
    // coalesces equivalent HTTP requests, and late results remain valid cache/data
    // input without leaving permanently shifted pending bits behind.
    pendingRequest = QBitArray(logicalCount, false);

    for (int index = 0; index < logicalCount; ++index) {
        if (!measured.testBit(index)) {
            heights.setValue(index, estimatedItemHeight(index));
        }
    }

    QHash<int, QWidget*> shiftedMaterialized;
    shiftedMaterialized.reserve(materialized.size());
    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        const int shiftedIndex = it.key() >= first ? it.key() + count : it.key();
        shiftedMaterialized.insert(shiftedIndex, it.value());
        widgetIndexes[it.value()] = shiftedIndex;
    }
    materialized = std::move(shiftedMaterialized);

    QSet<int> shiftedDirty;
    for (int index : std::as_const(dirtyGeometry)) {
        shiftedDirty.insert(index >= first ? index + count : index);
    }
    dirtyGeometry = std::move(shiftedDirty);

    if (seekTarget >= first) {
        seekTarget += count;
        ++seekGeneration;
    }

    QSignalBlocker blocker(verticalScrollBar());
    committingGeometry = true;
    updateScrollBarRange(oldOffset);
    if (hasViewportLock()) {
        restoreViewportLock();
    } else {
        restoreAnchor(anchor);
    }
    layoutMaterialized();
    committingGeometry = false;

    lastVisibleRange = {};
    lastMaterializedRange = {};
    scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
    emitRangeChanges();
}

void LongListWidget::removeItems(int first, int count)
{
    first = std::max(0, std::min(first, logicalCount));
    count = std::max(0, std::min(count, logicalCount - first));
    if (count == 0) {
        return;
    }

    const int removedLast = first + count - 1;
    const int newCount = logicalCount - count;

    ViewAnchor anchor = captureAnchor();
    if (anchor.kind == ViewAnchor::Item) {
        if (anchor.index > removedLast) {
            anchor.index -= count;
        } else if (anchor.index >= first) {
            if (newCount > 0) {
                anchor.index = std::min(first, newCount - 1);
                anchor.offsetInsideItem = 0;
            } else {
                anchor = ViewAnchor();
            }
        }
    }

    if (viewportLock.index > removedLast) {
        viewportLock.index -= count;
    } else if (viewportLock.index >= first) {
        releaseViewportLock(true);
    }

    if (seekTarget > removedLast) {
        seekTarget -= count;
        ++seekGeneration;
    } else if (seekTarget >= first) {
        clearSeek();
    }

    const qint64 oldOffset = contentOffset();

    QHash<int, QWidget*> shiftedMaterialized;
    shiftedMaterialized.reserve(materialized.size());
    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        const int index = it.key();
        QWidget* widget = it.value();
        if (index >= first && index <= removedLast) {
            if (widget) {
                widgetIndexes.remove(widget);
                widget->removeEventFilter(this);
                destroyItemWidget(index, widget);
            }
            continue;
        }

        const int shiftedIndex = index > removedLast ? index - count : index;
        shiftedMaterialized.insert(shiftedIndex, widget);
        if (widget) {
            widgetIndexes[widget] = shiftedIndex;
        }
    }
    materialized = std::move(shiftedMaterialized);

    QSet<int> shiftedDirty;
    for (int index : std::as_const(dirtyGeometry)) {
        if (index >= first && index <= removedLast) {
            continue;
        }
        shiftedDirty.insert(index > removedLast ? index - count : index);
    }
    dirtyGeometry = std::move(shiftedDirty);

    logicalCount = newCount;
    lastSynchronizedViewportRange = {};
    heights.remove(first, count);
    measured = removedBits(measured, first, count);
    available = removedBits(available, first, count);

    // Any outstanding range was expressed in the old coordinate system. Late
    // HTTP results can still populate the source by identity, but view-side
    // suppression must be rebuilt against the shifted logical indices.
    pendingRequest = QBitArray(logicalCount, false);

    QSignalBlocker blocker(verticalScrollBar());
    viewport()->setUpdatesEnabled(false);
    committingGeometry = true;
    updateScrollBarRange(oldOffset);
    if (hasViewportLock()) {
        restoreViewportLock();
    } else {
        restoreAnchor(anchor);
    }
    layoutMaterialized();
    committingGeometry = false;
    viewport()->setUpdatesEnabled(true);
    viewport()->update();

    lastVisibleRange = {};
    lastMaterializedRange = {};
    scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
    emitRangeChanges();
}

void LongListWidget::replaceItem(int index)
{
    if (index < 0 || index >= logicalCount) {
        return;
    }

    QWidget* previous = materialized.value(index, nullptr);
    const bool wasMaterialized = previous != nullptr;
    const int previousHeight = heights.value(index);
    const bool nowAvailable = isModelItemAvailable(index);

    dirtyGeometry.remove(index);
    available.setBit(index, nowAvailable);
    pendingRequest.clearBit(index);

    if (!wasMaterialized || !nowAvailable) {
        const ViewAnchor anchor = captureAnchor();
        const qint64 oldOffset = contentOffset();

        QSignalBlocker blocker(verticalScrollBar());
        committingGeometry = true;

        previous = materialized.take(index);
        if (previous) {
            if (hoveredWidget == previous) {
                hoveredWidget.clear();
            }
            widgetIndexes.remove(previous);
            previous->removeEventFilter(this);
            destroyItemWidget(index, previous);
        }

        measured.clearBit(index);
        heights.setValue(index, estimatedItemHeight(index));

        updateScrollBarRange(oldOffset);
        if (hasViewportLock()) {
            restoreViewportLock();
        } else {
            restoreAnchor(anchor);
        }
        layoutMaterialized();
        committingGeometry = false;
        scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
        return;
    }

    QWidget* replacement = createItemWidget(index);
    if (!replacement) {
        return;
    }
    replacement->setParent(viewport());

    const QString replacementIdentity = itemIdentity(replacement);
    const int firstHeight = naturalWidgetHeight(replacement);

    if (firstHeight != previousHeight) {
        // A freshly constructed complex row can report a transient sizeHint
        // until its queued child-layout work has run (rich text is a common
        // example). Keep the old identity visible and settle the replacement
        // off-screen for one event-loop turn before deciding that geometry
        // truly changed.
        stageReplacement(previous, replacement, replacementIdentity, index, firstHeight);
        return;
    }

    commitReplacement(previous, replacement, replacementIdentity, index);
}

void LongListWidget::stageReplacement(QWidget* previous,
                                      QWidget* replacement,
                                      const QString& replacementIdentity,
                                      int fallbackIndex,
                                      int observedHeight)
{
    QPointer<QWidget> previousGuard(previous);
    QPointer<QWidget> replacementGuard(replacement);

    QTimer::singleShot(0, this,
                       [this, previousGuard, replacementGuard,
                        replacementIdentity, fallbackIndex, observedHeight] {
        if (!replacementGuard) {
            return;
        }

        int index = replacementIdentity.isEmpty()
            ? fallbackIndex
            : indexOfItemIdentity(replacementIdentity);
        if (index < 0 || index >= logicalCount || !previousGuard
            || materialized.value(index, nullptr) != previousGuard.data()
            || !isModelItemAvailable(index)) {
            replacementGuard->deleteLater();
            return;
        }

        const int settledHeight = naturalWidgetHeight(replacementGuard.data());
        if (settledHeight != observedHeight) {
            // The first queued layout pass changed the answer. Give any
            // immediately-following child notification one more turn while the
            // old row remains visible; this is still hidden preparation only.
            QPointer<QWidget> previousAgain(previousGuard);
            QPointer<QWidget> replacementAgain(replacementGuard);
            QTimer::singleShot(0, this,
                               [this, previousAgain, replacementAgain,
                                replacementIdentity, fallbackIndex] {
                if (!replacementAgain) {
                    return;
                }
                int currentIndex = replacementIdentity.isEmpty()
                    ? fallbackIndex
                    : indexOfItemIdentity(replacementIdentity);
                if (currentIndex < 0 || currentIndex >= logicalCount
                    || !previousAgain
                    || materialized.value(currentIndex, nullptr)
                        != previousAgain.data()
                    || !isModelItemAvailable(currentIndex)) {
                    replacementAgain->deleteLater();
                    return;
                }
                naturalWidgetHeight(replacementAgain.data());
                commitReplacement(previousAgain.data(),
                                  replacementAgain.data(),
                                  replacementIdentity,
                                  currentIndex);
            });
            return;
        }

        commitReplacement(previousGuard.data(),
                          replacementGuard.data(),
                          replacementIdentity,
                          index);
    });
}

void LongListWidget::commitReplacement(QWidget* previous,
                                       QWidget* replacement,
                                       const QString& replacementIdentity,
                                       int fallbackIndex)
{
    if (!previous || !replacement) {
        if (replacement) {
            replacement->deleteLater();
        }
        return;
    }

    int index = replacementIdentity.isEmpty()
        ? fallbackIndex
        : indexOfItemIdentity(replacementIdentity);
    if (index < 0 || index >= logicalCount
        || materialized.value(index, nullptr) != previous
        || !isModelItemAvailable(index)) {
        replacement->deleteLater();
        return;
    }

    const ViewAnchor anchor = captureAnchor();
    const qint64 oldOffset = contentOffset();
    const int previousHeight = heights.value(index);
    const int replacementHeight = naturalWidgetHeight(replacement);

    measured.setBit(index, true);
    heights.setValue(index, replacementHeight);

    const int width = std::max(1, viewport()->width());
    if (replacement->height() != replacementHeight
        || replacement->width() != width) {
        replacement->resize(width, replacementHeight);
    }

    if (replacementHeight == previousHeight) {
        const QRect oldRect = previous->geometry();
        const qint64 y64 = heights.prefixHeight(index) - contentOffset();
        const int y = static_cast<int>(std::max<qint64>(
            INT_MIN, std::min<qint64>(INT_MAX, y64)));
        const QRect finalRect(0, y, width, replacementHeight);
        const QRect affected = oldRect.united(finalRect);

        if (hoveredWidget == previous) {
            hoveredWidget.clear();
        }
        widgetIndexes.remove(previous);
        previous->removeEventFilter(this);

        replacement->setGeometry(finalRect);
        replacement->installEventFilter(this);
        widgetIndexes.insert(replacement, index);
        materialized[index] = replacement;

        replacement->show();
        destroyItemWidget(index, previous);
        viewport()->update(affected);
        return;
    }

    QSignalBlocker blocker(verticalScrollBar());
    committingGeometry = true;

    materialized.remove(index);
    if (hoveredWidget == previous) {
        hoveredWidget.clear();
    }
    widgetIndexes.remove(previous);
    previous->removeEventFilter(this);

    replacement->installEventFilter(this);
    widgetIndexes.insert(replacement, index);
    materialized.insert(index, replacement);
    replacement->show();
    destroyItemWidget(index, previous);

    updateScrollBarRange(oldOffset);
    if (hasViewportLock()) {
        restoreViewportLock();
    } else {
        restoreAnchor(anchor);
    }
    layoutMaterialized();
    committingGeometry = false;

    scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
}

void LongListWidget::setDefaultItemHeight(int height)
{
    height = boundedHeight(height);
    if (defaultHeight == height) {
        return;
    }
    defaultHeight = height;
    lastSynchronizedViewportRange = {};

    for (int index = 0; index < logicalCount; ++index) {
        if (!measured.testBit(index)) {
            heights.setValue(index, estimatedItemHeight(index));
        }
    }
    commitGeometry(true);
    scheduleSync(RequestReason::Scroll);
}

void LongListWidget::setMaterializationLimit(int count)
{
    maxMaterializedItems = std::max(1, count);
    lastSynchronizedViewportRange = {};
    scheduleSync(RequestReason::Scroll);
}

void LongListWidget::setRequestBlockSize(int count)
{
    blockSize = std::max(1, count);
}

void LongListWidget::setPrefetchScreens(int screens)
{
    bufferScreens = std::max(0, screens);
    lastSynchronizedViewportRange = {};
    scheduleSync(RequestReason::Scroll);
}

void LongListWidget::setSeekDebounceMs(int milliseconds)
{
    seekDebounceInterval = std::max(0, milliseconds);
    seekTimer.setInterval(seekDebounceInterval);
}

QColor LongListWidget::hoverHighlightSurfaceColor() const
{
    const QColor base =
        viewport()->palette().color(viewport()->backgroundRole());
    const QColor highlight = palette().color(QPalette::Highlight);
    const qreal alpha = static_cast<qreal>(HoverHighlightAlpha) / 255.0;

    // paintEvent() applies Highlight with HoverHighlightAlpha over the viewport.
    // Return that final opaque color so an overlay widget does not apply the
    // alpha a second time over an already-highlighted row.
    return QColor::fromRgbF(
        highlight.redF() * alpha + base.redF() * (1.0 - alpha),
        highlight.greenF() * alpha + base.greenF() * (1.0 - alpha),
        highlight.blueF() * alpha + base.blueF() * (1.0 - alpha),
        1.0);
}

void LongListWidget::setHoverHighlightEnabled(bool enabled)
{
    if (hoverHighlightEnabled == enabled) {
        return;
    }
    hoverHighlightEnabled = enabled;
    if (!hoverHighlightEnabled) {
        hoverHighlightOverride.clear();
        if (hoveredWidget) {
            const int previousIndex =
                widgetIndexes.value(hoveredWidget.data(), -1);
            hoveredWidget.clear();
            emit hoveredItemChanged(previousIndex, -1);
        }
    }
    viewport()->update();
}

void LongListWidget::setHoverHighlightOverride(QWidget* widget)
{
    if (widget && !widgetIndexes.contains(widget)) {
        widget = nullptr;
    }
    if (hoverHighlightOverride.data() == widget) {
        return;
    }
    hoverHighlightOverride = widget;
    viewport()->update();
}

void LongListWidget::setRangeAvailable(int first, int last, bool isAvailable)
{
    if (logicalCount <= 0) {
        return;
    }
    first = std::max(0, first);
    last = std::min(logicalCount - 1, last);
    if (last < first) {
        return;
    }

    // A measured height belongs to the concrete logical identity that produced
    // it. If a provisional identity leaves a slot, that measurement must not be
    // reused as if it described the unknown item that will eventually occupy the
    // slot. Capture viewport intent before resetting any such height.
    const ViewAnchor anchor = !isAvailable ? captureAnchor() : ViewAnchor();
    const qint64 oldOffset = contentOffset();
    bool geometryChanged = false;
    bool availabilityProgress = false;

    for (int index = first; index <= last; ++index) {
        if (isAvailable && !available.testBit(index)) {
            availabilityProgress = true;
        }
        available.setBit(index, isAvailable);
        pendingRequest.clearBit(index);
        if (!isAvailable) {
            QWidget* widget = materialized.take(index);
            if (widget) {
                widgetIndexes.remove(widget);
                widget->removeEventFilter(this);
                destroyItemWidget(index, widget);
            }
            dirtyGeometry.remove(index);
            if (measured.testBit(index)) {
                measured.clearBit(index);
                heights.setValue(index, estimatedItemHeight(index));
                geometryChanged = true;
            }
        }
    }

    if (availabilityProgress) {
        ++availabilityRevision;
    }

    if (geometryChanged) {
        QSignalBlocker blocker(verticalScrollBar());
        viewport()->setUpdatesEnabled(false);
        committingGeometry = true;
        updateScrollBarRange(oldOffset);
        if (seekActive && seekTarget >= 0 && materialized.contains(seekTarget)) {
            restoreSeekTarget();
        } else if (hasViewportLock()) {
            restoreViewportLock();
        } else {
            restoreAnchor(anchor);
        }
        layoutMaterialized();
        committingGeometry = false;
        viewport()->setUpdatesEnabled(true);
        viewport()->update();
        emitRangeChanges();
    }

    scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
}

bool LongListWidget::isItemAvailable(int index) const
{
    return index >= 0 && index < logicalCount && available.testBit(index);
}

void LongListWidget::resolveSeekTarget(int index, quint64 generation)
{
    if (!seekActive || generation != seekGeneration || index < 0 || index >= logicalCount) return;
    seekTarget = index;
    QSignalBlocker blocker(verticalScrollBar());
    restoreSeekTarget();
    scheduleSync(RequestReason::Seek);
}

void LongListWidget::finishRangeRequest(int first, int last)
{
    clearPendingRequest(first, last);
    emit rangeRequestFinished(first, last);

    if (!hasMissingVisibleItems()) {
        observedAvailabilityRevision = availabilityRevision;
        return;
    }

    if (availabilityRevision != observedAvailabilityRevision) {
        // Concrete off-target progress can establish a better semantic anchor.
        // Re-evaluate once after pending suppression is released. A completion
        // with no progress deliberately does not schedule another transport
        // attempt: source-side cursor convergence owns that responsibility and
        // the view must never turn a failed seek into an HTTP polling loop.
        observedAvailabilityRevision = availabilityRevision;
        scheduleSync(seekActive ? RequestReason::Seek : RequestReason::Scroll);
    }
}

void LongListWidget::itemsChanged(int first, int last)
{
    first = std::max(0, first);
    last = std::min(logicalCount - 1, last);
    for (int index = first; index <= last; ++index) {
        if (materialized.contains(index)) {
            dirtyGeometry.insert(index);
        }
    }
    scheduleGeometryCommit();
}

void LongListWidget::commitItemGeometryNow(int index)
{
    QWidget* widget = materialized.value(index, nullptr);
    if (!widget || index < 0 || index >= logicalCount) {
        return;
    }

    if (committingGeometry || synchronizing) {
        scheduleGeometryCommit(index);
        return;
    }

    const ViewAnchor anchor = captureAnchor();
    const qint64 oldOffset = contentOffset();

    QSignalBlocker blocker(verticalScrollBar());
    committingGeometry = true;

    dirtyGeometry.remove(index);
    const bool geometryChanged = measureWidget(index, widget);

    if (geometryChanged) {
        updateScrollBarRange(oldOffset);
        if (seekActive && seekTarget >= 0 && materialized.contains(seekTarget)) {
            restoreSeekTarget();
        } else if (hasViewportLock()) {
            restoreViewportLock();
        } else {
            restoreAnchor(anchor);
        }
        layoutMaterialized();
    }

    committingGeometry = false;

    if (geometryChanged) {
        if (seekActive) {
            scheduleSync(RequestReason::Seek);
        } else {
            scheduleViewportSyncIfNeeded(RequestReason::Scroll);
        }
        emitRangeChanges();
    }
}

QWidget* LongListWidget::itemWidget(int index) const
{
    return materialized.value(index, nullptr);
}

QVector<int> LongListWidget::materializedIndices() const
{
    QVector<int> result;
    result.reserve(materialized.size());
    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        result.push_back(it.key());
    }
    std::sort(result.begin(), result.end());
    return result;
}

LongListWidget::Range LongListWidget::visibleRange() const
{
    Range result;
    if (logicalCount <= 0 || viewport()->height() <= 0 || heights.totalHeight() <= 0) {
        return result;
    }

    const qint64 top = contentOffset();
    const qint64 bottom = std::min<qint64>(heights.totalHeight() - 1,
        top + std::max(0, viewport()->height() - 1));
    result.first = heights.indexAtPixel(top);
    result.last = heights.indexAtPixel(bottom);
    return result;
}

LongListWidget::Range LongListWidget::materializedRange() const
{
    Range result;
    const QVector<int> indices = materializedIndices();
    if (!indices.isEmpty()) {
        result.first = indices.first();
        result.last = indices.last();
    }
    return result;
}

qint64 LongListWidget::contentHeight() const
{
    return heights.totalHeight();
}

int LongListWidget::viewportCenterIndex() const
{
    return viewport()->height() > 0
        ? indexAtViewportPosition(viewport()->height() / 2)
        : -1;
}

bool LongListWidget::captureSemanticAnchorFraction(int index,
                                                    SemanticAnchorPoint point,
                                                    qreal& fraction) const
{
    if (index < 0 || index >= logicalCount || viewport()->height() <= 0) {
        return false;
    }
    const qint64 h = heights.value(index);
    const qint64 itemAnchor = heights.prefixHeight(index)
        + (point == SemanticAnchorPoint::ItemCenter ? h / 2 : h);
    const qint64 viewportAnchor = contentOffset()
        + (point == SemanticAnchorPoint::ItemCenter
            ? viewport()->height() / 2 : viewport()->height());
    fraction = static_cast<qreal>(itemAnchor - viewportAnchor)
        / static_cast<qreal>(viewport()->height());
    return true;
}

LongListWidget::ItemVisibilities LongListWidget::itemVisibility(int index) const
{
    QWidget* widget = materialized.value(index, nullptr);
    if (!widget || viewport()->height() <= 0) {
        return {};
    }

    const int top = widget->y();
    const int bottom = top + widget->height();
    const int viewportHeight = viewport()->height();

    ItemVisibilities visibility;
    if (bottom > 0 && top < viewportHeight) {
        visibility |= ItemVisibility::Body;
    }
    if (top >= 0 && top < viewportHeight) {
        visibility |= ItemVisibility::Top;
    }
    if (bottom > 0 && bottom <= viewportHeight) {
        visibility |= ItemVisibility::Bottom;
    }
    return visibility;
}

int LongListWidget::indexAtViewportPosition(int viewportY) const
{
    if (logicalCount <= 0) {
        return -1;
    }
    const qint64 pixel = contentOffset() + std::max(0, viewportY);
    return heights.indexAtPixel(pixel);
}

qint64 LongListWidget::alignedContentOffset(int index, Alignment alignment) const
{
    if (index < 0 || index >= logicalCount) {
        return contentOffset();
    }

    const qint64 itemTop = heights.prefixHeight(index);
    const qint64 itemBottom = heights.prefixHeight(index + 1);
    const qint64 itemHeight = heights.value(index);
    const qint64 viewportHeight = std::max(0, viewport()->height());
    const qint64 currentTop = contentOffset();
    const qint64 currentBottom = currentTop + viewportHeight;

    switch (alignment) {
    case Alignment::Top:
        return itemTop;
    case Alignment::Center:
        // A widget that does not fit has no useful visual centre: clipping its
        // beginning hides the context the user navigated to. Show its beginning
        // instead, while ordinary rows stay genuinely centred.
        if (itemHeight >= viewportHeight) {
            return itemTop;
        }
        return itemTop - (viewportHeight - itemHeight) / 2;
    case Alignment::Bottom:
        return itemBottom - viewportHeight;
    case Alignment::EnsureVisible:
        if (itemTop < currentTop) {
            return itemTop;
        }
        if (itemBottom > currentBottom) {
            return itemBottom - viewportHeight;
        }
        return currentTop;
    }
    return currentTop;
}

void LongListWidget::scrollToIndex(int index, Alignment alignment)
{
    if (index < 0 || index >= logicalCount) {
        return;
    }

    stopScrollAnimation();

    if (hasViewportLock() && viewportLock.index != index) {
        releaseViewportLock(true);
    }

    clearSeek();
    const qint64 target = alignedContentOffset(index, alignment);

    internalScrollChange = true;
    verticalScrollBar()->setValue(scrollValueForContentOffset(target));
    internalScrollChange = false;
    layoutMaterialized();
    if (hasViewportLock() && viewportLock.index == index) {
        captureViewportLockFraction();
    }
    scheduleSync(RequestReason::EnsureVisible);
}

void LongListWidget::scrollToEnd()
{
    stopScrollAnimation();
    if (hasViewportLock()) {
        releaseViewportLock(true);
    }
    clearSeek();
    internalScrollChange = true;
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    internalScrollChange = false;
    layoutMaterialized();
    scheduleSync(RequestReason::EnsureVisible);
}

void LongListWidget::scrollToEndAnimated(int durationMs)
{
    if (hasViewportLock()) {
        releaseViewportLock(true);
    }
    clearSeek();

    QScrollBar* bar = verticalScrollBar();
    const qint64 distance = maximumContentOffset() - contentOffset();
    if (durationMs <= 0 || distance <= 0) {
        scrollToEnd();
        return;
    }

    // Long-distance animation is disorienting and keeps unrelated history in
    // motion. The animated path is intended for a nearby appended tail (for
    // example one optimistic outgoing row just below the viewport).
    if (distance > std::max(1, viewport()->height())) {
        scrollToEnd();
        return;
    }

    stopScrollAnimation();
    scrollAnimationStartValue = bar->value();
    scrollAnimation.setDuration(std::max(1, durationMs));
    scrollAnimation.start();
}

bool LongListWidget::lockViewportToItem(int index,
                                        Alignment alignment,
                                        int quietPeriodMs)
{
    if (index < 0 || index >= logicalCount) {
        clearViewportLock();
        return false;
    }

    // Replace any previous lock without reporting an intermediate unlock to the
    // domain layer. Geometry outside the target keeps the target at the current
    // screen Y; a later target-height change re-applies this semantic alignment.
    releaseViewportLock(false);
    scrollToIndex(index, alignment);

    viewportLock.index = index;
    viewportLock.itemHeight = heights.value(index);
    viewportLock.alignment = alignment;
    viewportLock.quietPeriodMs = std::max(0, quietPeriodMs);
    captureViewportLockFraction();
    touchViewportLock();
    return true;
}

bool LongListWidget::lockViewportToSemanticAnchor(int index,
                                                   SemanticAnchorPoint point,
                                                   qreal fraction,
                                                   int quietPeriodMs)
{
    if (!std::isfinite(static_cast<double>(fraction))
        || !lockViewportToItem(index,
                              point == SemanticAnchorPoint::ItemBottom
                                  ? Alignment::Bottom : Alignment::Center,
                              quietPeriodMs)) {
        return false;
    }
    viewportLock.explicitSemanticAnchor = true;
    viewportLock.semanticPoint = point;
    viewportLock.semanticFraction = static_cast<long double>(fraction);
    QSignalBlocker blocker(verticalScrollBar());
    restoreViewportLock();
    layoutMaterialized();
    viewport()->update();
    scheduleSync(RequestReason::EnsureVisible);
    return true;
}

bool LongListWidget::remapViewportLockedItem(int index)
{
    if (!hasViewportLock() || index < 0 || index >= logicalCount) {
        return false;
    }
    if (viewportLock.index == index) {
        touchViewportLock();
        return true;
    }

    // An authoritative source remap changes identity, not navigation intent.
    // Adopt the destination's current height before restoring so the remap
    // itself preserves screen Y. Any later real height change can then trigger
    // semantic realignment normally.
    viewportLock.index = index;
    viewportLock.itemHeight = heights.value(index);
    QSignalBlocker blocker(verticalScrollBar());
    viewport()->setUpdatesEnabled(false);
    restoreViewportLock();
    layoutMaterialized();
    viewport()->setUpdatesEnabled(true);
    viewport()->update();
    touchViewportLock();
    scheduleSync(RequestReason::EnsureVisible);
    return true;
}

void LongListWidget::clearViewportLock()
{
    releaseViewportLock(true);
}

void LongListWidget::destroyItemWidget(int, QWidget* widget)
{
    if (widget) {
        widget->hide();
        widget->deleteLater();
    }
}

int LongListWidget::estimatedItemHeight(int) const
{
    return defaultHeight;
}

bool LongListWidget::eventFilter(QObject* watched, QEvent* event)
{
    const auto it = widgetIndexes.constFind(watched);
    if (it != widgetIndexes.cend()) {
        if (event->type() == QEvent::Enter && hoverHighlightEnabled) {
            QWidget* current = qobject_cast<QWidget*>(watched);
            if (current && hoveredWidget.data() != current) {
                const int previousIndex = widgetIndexes.value(hoveredWidget.data(), -1);
                hoveredWidget = current;
                emit hoveredItemChanged(previousIndex, it.value());
            }
            viewport()->update();
        } else if ((event->type() == QEvent::Leave || event->type() == QEvent::Hide)
                   && hoveredWidget.data() == watched) {
            const int previousIndex = it.value();
            hoveredWidget.clear();
            emit hoveredItemChanged(previousIndex, -1);
            viewport()->update();
        }

        if ((event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize)
            && !synchronizing && !committingGeometry) {
            scheduleGeometryCommit(it.value());
        }
    }
    return QAbstractScrollArea::eventFilter(watched, event);
}

void LongListWidget::paintEvent(QPaintEvent* event)
{
    QAbstractScrollArea::paintEvent(event);

    QWidget* highlightWidget = hoverHighlightOverride
        ? hoverHighlightOverride.data() : hoveredWidget.data();
    if (!hoverHighlightEnabled || !highlightWidget
        || !highlightWidget->isVisible()) {
        return;
    }

    QColor background = palette().color(QPalette::Highlight);
    background.setAlpha(HoverHighlightAlpha);
    QPainter painter(viewport());
    painter.fillRect(highlightWidget->geometry(), background);
}

void LongListWidget::resizeEvent(QResizeEvent* event)
{
    const ViewAnchor anchor = captureAnchor();
    lastSynchronizedViewportRange = {};
    QAbstractScrollArea::resizeEvent(event);

    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        dirtyGeometry.insert(it.key());
    }

    const qint64 oldOffset = contentOffset();
    QSignalBlocker blocker(verticalScrollBar());
    viewport()->setUpdatesEnabled(false);
    committingGeometry = true;
    for (int index : std::as_const(dirtyGeometry)) {
        if (QWidget* widget = materialized.value(index, nullptr)) {
            measureWidget(index, widget);
        }
    }
    dirtyGeometry.clear();
    updateScrollBarRange(oldOffset);
    if (hasViewportLock()) {
        // If only viewport height changed, restoreViewportLock() preserves the
        // old relative Y. If width reflow also changed the target's own height,
        // it re-applies the semantic alignment instead.
        restoreViewportLock();
    } else {
        restoreAnchor(anchor);
    }
    layoutMaterialized();
    committingGeometry = false;
    viewport()->setUpdatesEnabled(true);
    viewport()->update();

    emitRangeChanges(VisibilityChangeReason::LayoutChange);
    scheduleSync(RequestReason::Scroll);
}

void LongListWidget::wheelEvent(QWheelEvent* event)
{
    clearSeek();
    const QPoint pixelDelta = event->pixelDelta();
    int delta = pixelDelta.y();
    if (delta == 0) {
        const QPoint angleDelta = event->angleDelta();
        delta = angleDelta.y() / 120 * std::max(24, defaultHeight / 2);
    }

    if (delta != 0) {
        noteUserScrollStarted();
        wheelInProgress = true;
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta);
        wheelInProgress = false;
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void LongListWidget::scheduleSync(RequestReason reason)
{
    if (reason == RequestReason::Seek
        || pendingSyncReason == RequestReason::Initial
        || reason == RequestReason::EnsureVisible) {
        pendingSyncReason = reason;
    } else if (pendingSyncReason != RequestReason::Seek) {
        pendingSyncReason = reason;
    }
    if (!syncTimer.isActive()) {
        syncTimer.start(0);
    }
}

void LongListWidget::scheduleViewportSyncIfNeeded(RequestReason reason, int scrollValue)
{
    if (seekActive) {
        scheduleSync(RequestReason::Seek);
        return;
    }

    Range desired = desiredRangeForViewport(scrollValue);
    const int preferredCenter =
        desired.isValid() ? (desired.first + desired.last) / 2 : -1;
    desired = clampToBudget(desired, preferredCenter);
    if (!sameRange(desired, lastSynchronizedViewportRange)) {
        scheduleSync(reason);
    }
}

void LongListWidget::synchronize()
{
    if (synchronizing || logicalCount <= 0) {
        if (logicalCount <= 0) {
            lastSynchronizedViewportRange = {};
        }
        emitRangeChanges();
        return;
    }

    synchronizing = true;
    const RequestReason reason = seekActive ? RequestReason::Seek : pendingSyncReason;
    pendingSyncReason = RequestReason::Scroll;
    Range desired = seekActive ? desiredRangeForSeek() : desiredRangeForViewport();
    const int preferredCenter = seekActive && seekTarget >= 0
        ? seekTarget
        : (desired.isValid() ? (desired.first + desired.last) / 2 : -1);
    desired = clampToBudget(desired, preferredCenter);
    if (!seekActive) {
        lastSynchronizedViewportRange = desired;
    }
    synchronizeRange(desired, reason, seekActive ? seekGeneration : 0, seekActive);
    synchronizing = false;

    // Measuring the first real rows can expose additional logical rows around
    // a seek target. Re-evaluate that changed geometry even if the source has
    // already finished the original demand; no further availability signal is
    // guaranteed. This is one layout update, not a retry of an unchanged gap.
    const Range measuredDesiredRange = clampToBudget(
        seekActive ? desiredRangeForSeek() : desiredRangeForViewport(), preferredCenter);
    if (!sameRange(desired, measuredDesiredRange)) {
        scheduleSync(reason);
    }
}

void LongListWidget::synchronizeRange(const Range& desired,
                                      RequestReason reason,
                                      quint64 generation,
                                      bool centerSeekTarget)
{
    if (!desired.isValid()) {
        emitRangeChanges();
        return;
    }

    const ViewAnchor anchor = captureAnchor();
    const qint64 oldOffset = contentOffset();
    QSignalBlocker blocker(verticalScrollBar());

    // synchronizeRange() is one synchronous event-loop transaction. Painting
    // cannot occur between materialization, anchor restoration and final child
    // geometry unless we explicitly re-enter the event loop, which we do not.
    // Disabling/re-enabling viewport updates here only invalidates the entire
    // viewport and makes every post repaint on ordinary tail materialization.
    materializeAvailable(desired);
    evictOutside(desired, centerSeekTarget ? seekTarget : (desired.first + desired.last) / 2);
    updateScrollBarRange(oldOffset);
    if (centerSeekTarget && seekTarget >= 0 && materialized.contains(seekTarget)) {
        restoreSeekTarget();
    } else if (hasViewportLock()) {
        restoreViewportLock();
    } else {
        restoreAnchor(anchor);
    }
    layoutMaterialized();

    requestMissing(desired, reason, generation);
    emitRangeChanges(centerSeekTarget || reason == RequestReason::EnsureVisible
                         ? VisibilityChangeReason::ProgrammaticScroll
                         : VisibilityChangeReason::LayoutChange);
}

LongListWidget::Range LongListWidget::desiredRangeForViewport(int scrollValue) const
{
    Range result;
    if (logicalCount <= 0 || viewport()->height() <= 0) {
        return result;
    }

    const qint64 top = scrollValue >= 0
        ? contentOffsetForScrollValue(scrollValue)
        : contentOffset();
    const qint64 buffer = static_cast<qint64>(viewport()->height()) * bufferScreens;
    const qint64 firstPixel = std::max<qint64>(0, top - buffer);
    const qint64 lastPixel = std::min<qint64>(heights.totalHeight() - 1,
        top + viewport()->height() + buffer - 1);
    result.first = heights.indexAtPixel(firstPixel);
    result.last = heights.indexAtPixel(std::max(firstPixel, lastPixel));

    // Estimate screen capacity from the average height of the visible rows.
    // Unknown rows retain their estimated height; measurements refine the margin
    // through the normal geometry sync. Keep at least one neighbour for tall posts.
    Range visible;
    const qint64 visibleBottom = std::min<qint64>(heights.totalHeight() - 1,
        top + std::max(0, viewport()->height() - 1));
    visible.first = heights.indexAtPixel(top);
    visible.last = heights.indexAtPixel(visibleBottom);
    if (visible.isValid()) {
        const qint64 rowHeightSum = heights.prefixHeight(visible.last + 1)
            - heights.prefixHeight(visible.first);
        const qint64 numerator = qint64(viewport()->height()) * visible.count();
        const qint64 denominator = 2 * std::max<qint64>(1, rowHeightSum);
        const int margin = static_cast<int>(std::clamp<qint64>(
            (numerator + denominator - 1) / denominator, 1, logicalCount));
        result.first = std::min(result.first,
                                std::max(0, visible.first - margin));
        result.last = std::max(result.last,
                               std::min(logicalCount - 1,
                                        visible.last + margin));
    }
    return result;
}

LongListWidget::Range LongListWidget::desiredRangeForSeek() const
{
    if (!seekActive || seekTarget < 0 || logicalCount <= 0) {
        return {};
    }

    if (!materialized.contains(seekTarget)) {
        return boundedAround(seekTarget, blockSize);
    }

    const qint64 targetTop = heights.prefixHeight(seekTarget);
    const qint64 targetCenter = targetTop + heights.value(seekTarget) / 2;
    // restoreSeekTarget() clamps the centered offset at the source boundaries.
    // Use that same effective viewport: a symmetric window about the target
    // misses visible rows on the opposite side when centering is impossible.
    const qint64 top = contentOffsetForScrollValue(scrollValueForContentOffset(
        targetCenter - viewport()->height() / 2));
    const qint64 buffer = static_cast<qint64>(viewport()->height()) * bufferScreens;
    const qint64 firstPixel = std::max<qint64>(0, top - buffer);
    const qint64 lastPixel = std::min<qint64>(heights.totalHeight() - 1,
        top + viewport()->height() + buffer - 1);

    Range result;
    result.first = heights.indexAtPixel(firstPixel);
    result.last = heights.indexAtPixel(lastPixel);
    const Range minimum = boundedAround(seekTarget, blockSize);
    if (minimum.isValid()) {
        result.first = std::min(result.first, minimum.first);
        result.last = std::max(result.last, minimum.last);
    }
    return result;
}

LongListWidget::Range LongListWidget::boundedAround(int center, int count) const
{
    Range result;
    if (logicalCount <= 0 || center < 0 || center >= logicalCount || count <= 0) {
        return result;
    }
    count = std::min(count, logicalCount);
    int first = center - (count - 1) / 2;
    first = std::max(0, std::min(first, logicalCount - count));
    result.first = first;
    result.last = first + count - 1;
    return result;
}

LongListWidget::Range LongListWidget::clampToBudget(const Range& range, int preferredCenter) const
{
    if (!range.isValid() || range.count() <= maxMaterializedItems) {
        return range;
    }
    preferredCenter = std::max(range.first, std::min(range.last, preferredCenter));
    Range result = boundedAround(preferredCenter, maxMaterializedItems);
    result.first = std::max(result.first, range.first);
    result.last = std::min(result.last, range.last);
    return result;
}

void LongListWidget::materializeAvailable(const Range& range)
{
    for (int index = range.first; index <= range.last; ++index) {
        if (!available.testBit(index) || materialized.contains(index)) {
            continue;
        }

        QWidget* widget = createItemWidget(index);
        if (!widget) {
            continue;
        }
        widget->setParent(viewport());
        widget->installEventFilter(this);
        widgetIndexes.insert(widget, index);
        materialized.insert(index, widget);
        widget->show();

        const int width = std::max(1, viewport()->width());
        widget->resize(width, std::max(1, heights.value(index)));
        if (QLayout* layout = widget->layout()) {
            layout->activate();
        }
        measureWidget(index, widget);
    }
}

void LongListWidget::evictOutside(const Range& keepRange, int preferredCenter)
{
    // maxMaterializedItems is a residency budget, not the size of the current
    // viewport window. Keep already-created widgets while there is budget so
    // short chats and nearby back-scrolls never churn rows unnecessarily.
    int excess = static_cast<int>(materialized.size()) - maxMaterializedItems;
    if (excess <= 0) {
        return;
    }

    QVector<int> candidates;
    const QVector<int> current = materializedIndices();
    candidates.reserve(materialized.size());
    for (int index : current) {
        if (!keepRange.contains(index)) {
            candidates.push_back(index);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [preferredCenter](int lhs, int rhs) {
        const int lhsDistance = lhs < preferredCenter
            ? preferredCenter - lhs : lhs - preferredCenter;
        const int rhsDistance = rhs < preferredCenter
            ? preferredCenter - rhs : rhs - preferredCenter;
        return lhsDistance > rhsDistance;
    });

    for (int index : candidates) {
        if (excess <= 0) {
            break;
        }
        QWidget* widget = materialized.take(index);
        if (!widget) {
            continue;
        }
        widgetIndexes.remove(widget);
        widget->removeEventFilter(this);
        destroyItemWidget(index, widget);
        --excess;
    }
}

void LongListWidget::layoutMaterialized()
{
    if (materialized.isEmpty()) {
        return;
    }

    const qint64 top = contentOffset();
    const int width = std::max(1, viewport()->width());
    for (auto it = materialized.begin(); it != materialized.end(); ++it) {
        const int index = it.key();
        QWidget* widget = it.value();
        if (!widget) {
            continue;
        }
        const qint64 y64 = heights.prefixHeight(index) - top;
        const int y = static_cast<int>(std::max<qint64>(INT_MIN,
            std::min<qint64>(INT_MAX, y64)));
        widget->setGeometry(0, y, width, heights.value(index));
    }
}

int LongListWidget::naturalWidgetHeight(QWidget* widget)
{
    if (!widget) {
        return 0;
    }

    const int width = std::max(1, viewport()->width());
    if (widget->width() != width) {
        widget->resize(width, std::max(1, widget->height()));
    }
    if (QLayout* layout = widget->layout()) {
        layout->activate();
    }
    widget->updateGeometry();

    const int heightForWidth = widget->heightForWidth(width);
    return std::max({
        1,
        heightForWidth,
        widget->sizeHint().height(),
        widget->minimumSizeHint().height(),
    });
}

bool LongListWidget::measureWidget(int index, QWidget* widget)
{
    if (!widget || index < 0 || index >= logicalCount) {
        return false;
    }

    const int width = std::max(1, viewport()->width());
    const int height = naturalWidgetHeight(widget);
    const bool changed = !measured.testBit(index)
        || heights.value(index) != height;
    heights.setValue(index, height);
    measured.setBit(index, true);

    if (widget->height() != height) {
        widget->resize(width, height);
    }
    return changed;
}

void LongListWidget::scheduleGeometryCommit(int index)
{
    if (index >= 0) {
        dirtyGeometry.insert(index);
    }
    if (!geometryTimer.isActive()) {
        geometryTimer.start(0);
    }
}

void LongListWidget::commitGeometry(bool heightIndexChanged)
{
    if (committingGeometry || synchronizing) {
        if (!geometryTimer.isActive()) {
            geometryTimer.start(0);
        }
        return;
    }

    const ViewAnchor anchor = captureAnchor();
    const qint64 oldOffset = contentOffset();
    QSignalBlocker blocker(verticalScrollBar());
    committingGeometry = true;

    bool geometryChanged = heightIndexChanged;
    for (int index : std::as_const(dirtyGeometry)) {
        if (QWidget* widget = materialized.value(index, nullptr)) {
            geometryChanged = measureWidget(index, widget) || geometryChanged;
        }
    }
    dirtyGeometry.clear();

    if (geometryChanged) {
        updateScrollBarRange(oldOffset);
        if (seekActive && seekTarget >= 0 && materialized.contains(seekTarget)) {
            restoreSeekTarget();
        } else if (hasViewportLock()) {
            restoreViewportLock();
        } else {
            restoreAnchor(anchor);
        }
        layoutMaterialized();
    }

    committingGeometry = false;

    // Structural HeightIndex changes (item-count/default-estimate changes)
    // are real geometry changes even when no concrete row is dirty. They must
    // update the scrollbar range. LayoutRequest/Resize, by contrast, is only a
    // hint that geometry *might* have changed.
    // Hover affordances and other child-widget activity can emit that hint while
    // the row's measured height remains identical. A no-op remeasure must not
    // wake sparse-range demand, otherwise ordinary mouse movement can turn an
    // unresolved off-screen gap into repeated transport requests.
    if (!geometryChanged) {
        return;
    }

    viewport()->update();
    if (seekActive) {
        scheduleSync(RequestReason::Seek);
    } else {
        scheduleViewportSyncIfNeeded(RequestReason::Scroll);
    }
    emitRangeChanges();
}

void LongListWidget::requestMissing(const Range& desired,
                                    RequestReason reason,
                                    quint64 generation)
{
    if (!desired.isValid()) {
        return;
    }

    // Express viewport demand, not transport pagination. Concrete sources know
    // whether a contiguous missing run is best served by page zero, a cursor,
    // a tail request or a random seek. Splitting it here into blockSize-aligned
    // chunks can make several source decisions race before the first exact
    // boundary response has established an anchor.
    int index = desired.first;
    while (index <= desired.last) {
        while (index <= desired.last
               && (available.testBit(index) || pendingRequest.testBit(index))) {
            ++index;
        }
        if (index > desired.last) {
            break;
        }

        const int requestFirst = index;
        while (index <= desired.last
               && !available.testBit(index) && !pendingRequest.testBit(index)) {
            pendingRequest.setBit(index, true);
            ++index;
        }
        emit rangeRequested(requestFirst, index - 1, reason, generation);
    }
}

void LongListWidget::clearPendingRequest(int first, int last)
{
    first = std::max(0, first);
    last = std::min(logicalCount - 1, last);
    for (int index = first; index <= last; ++index) {
        pendingRequest.clearBit(index);
    }
}

bool LongListWidget::hasMissingItems(const Range& range) const
{
    if (!range.isValid()) {
        return false;
    }
    const int first = std::max(0, range.first);
    const int last = std::min(logicalCount - 1, range.last);
    for (int index = first; index <= last; ++index) {
        if (!available.testBit(index)) {
            return true;
        }
    }
    return false;
}

bool LongListWidget::hasMissingVisibleItems() const
{
    return hasMissingItems(visibleRange());
}



LongListWidget::ViewAnchor LongListWidget::captureAnchor() const
{
    ViewAnchor anchor;
    if (logicalCount <= 0) {
        return anchor;
    }

    const qint64 offset = contentOffset();
    if (verticalScrollBar()->maximum() > verticalScrollBar()->minimum()
        && verticalScrollBar()->value() == verticalScrollBar()->maximum()) {
        anchor.kind = ViewAnchor::Bottom;
        return anchor;
    }

    const int index = heights.indexAtPixel(offset);
    if (index < 0) {
        return anchor;
    }
    anchor.kind = ViewAnchor::Item;
    anchor.index = index;
    anchor.offsetInsideItem = offset - heights.prefixHeight(index);
    return anchor;
}

void LongListWidget::restoreAnchor(const ViewAnchor& anchor)
{
    qint64 offset = contentOffset();
    if (anchor.kind == ViewAnchor::Bottom) {
        offset = maximumContentOffset();
    } else if (anchor.kind == ViewAnchor::Item
               && anchor.index >= 0 && anchor.index < logicalCount) {
        offset = heights.prefixHeight(anchor.index) + anchor.offsetInsideItem;
    }
    verticalScrollBar()->setValue(scrollValueForContentOffset(offset));
}

void LongListWidget::restoreSeekTarget()
{
    if (!seekActive || seekTarget < 0 || seekTarget >= logicalCount) {
        return;
    }
    const qint64 targetCenter = heights.prefixHeight(seekTarget)
        + heights.value(seekTarget) / 2;
    const qint64 offset = targetCenter - viewport()->height() / 2;
    verticalScrollBar()->setValue(scrollValueForContentOffset(offset));
}

void LongListWidget::captureViewportLockFraction()
{
    if (!hasViewportLock() || viewportLock.index >= logicalCount) {
        return;
    }
    const int viewportHeight = std::max(1, viewport()->height());
    const qint64 itemTopY = heights.prefixHeight(viewportLock.index) - contentOffset();
    viewportLock.itemTopFraction = static_cast<long double>(itemTopY)
        / static_cast<long double>(viewportHeight);
}

void LongListWidget::restoreViewportLock()
{
    if (!hasViewportLock() || viewportLock.index >= logicalCount) {
        return;
    }

    const int currentItemHeight = heights.value(viewportLock.index);
    if (currentItemHeight != viewportLock.itemHeight
        && !viewportLock.explicitSemanticAnchor) {
        // The target itself reflowed. Keeping its old top Y would make a row
        // that grew after navigation spill out of the viewport, even though it
        // could be centred with the now-authoritative height. Re-apply the
        // semantic alignment, then make that new placement the lock anchor.
        viewportLock.itemHeight = currentItemHeight;
        verticalScrollBar()->setValue(scrollValueForContentOffset(
            alignedContentOffset(viewportLock.index, viewportLock.alignment)));
        captureViewportLockFraction();
        return;
    }

    viewportLock.itemHeight = currentItemHeight;
    const int viewHeight = std::max(1, viewport()->height());
    const long double desiredViewportY = viewportLock.explicitSemanticAnchor
        ? ((viewportLock.semanticPoint == SemanticAnchorPoint::ItemBottom
                ? static_cast<long double>(viewHeight)
                : static_cast<long double>(viewHeight) / 2)
            + viewportLock.semanticFraction * viewHeight)
        : (viewportLock.itemTopFraction * viewHeight);
    const qint64 itemAnchor = heights.prefixHeight(viewportLock.index)
        + (viewportLock.explicitSemanticAnchor
            ? (viewportLock.semanticPoint == SemanticAnchorPoint::ItemBottom
                ? currentItemHeight : currentItemHeight / 2)
            : 0);
    const qint64 targetOffset = itemAnchor
        - static_cast<qint64>(std::llround(desiredViewportY));
    verticalScrollBar()->setValue(scrollValueForContentOffset(targetOffset));
}

void LongListWidget::touchViewportLock()
{
    if (!hasViewportLock()) {
        return;
    }
    viewportLockTimer.stop();
    if (viewportLock.quietPeriodMs > 0) {
        viewportLockTimer.start(viewportLock.quietPeriodMs);
    }
}

void LongListWidget::releaseViewportLock(bool notify)
{
    const bool wasLocked = hasViewportLock();
    viewportLockTimer.stop();
    viewportLock = ViewportLock();
    if (wasLocked && notify) {
        emit viewportLockReleased();
    }
}

void LongListWidget::noteUserScrollStarted()
{
    stopScrollAnimation();
    releaseViewportLock(true);
    // A new viewport consumes only progress observed after this gesture.
    observedAvailabilityRevision = availabilityRevision;
    emit userScrollStarted();
}

void LongListWidget::stopScrollAnimation()
{
    if (scrollAnimation.state() != QAbstractAnimation::Stopped) {
        scrollAnimation.stop();
    }
}

qint64 LongListWidget::maximumContentOffset() const
{
    return std::max<qint64>(0, heights.totalHeight() - viewport()->height());
}

qint64 LongListWidget::contentOffset() const
{
    return contentOffsetForScrollValue(verticalScrollBar()->value());
}

int LongListWidget::scrollValueForContentOffset(qint64 offset) const
{
    const qint64 maximumOffset = maximumContentOffset();
    const int barMaximum = verticalScrollBar()->maximum();
    if (maximumOffset <= 0 || barMaximum <= 0) {
        return 0;
    }
    offset = std::max<qint64>(0, std::min(offset, maximumOffset));
    if (maximumOffset <= INT_MAX) {
        return static_cast<int>(offset);
    }
    const long double fraction = static_cast<long double>(offset)
        / static_cast<long double>(maximumOffset);
    return static_cast<int>(std::llround(fraction * barMaximum));
}

qint64 LongListWidget::contentOffsetForScrollValue(int value) const
{
    const qint64 maximumOffset = maximumContentOffset();
    const int barMaximum = verticalScrollBar()->maximum();
    if (maximumOffset <= 0 || barMaximum <= 0) {
        return 0;
    }
    value = std::max(0, std::min(value, barMaximum));
    if (maximumOffset <= INT_MAX) {
        return value;
    }
    const long double fraction = static_cast<long double>(value)
        / static_cast<long double>(barMaximum);
    return static_cast<qint64>(std::llround(fraction * maximumOffset));
}

void LongListWidget::updateScrollBarRange(qint64 preservedOffset)
{
    const qint64 maximumOffset = maximumContentOffset();
    const int maximum = static_cast<int>(std::min<qint64>(maximumOffset, INT_MAX));
    QScrollBar* bar = verticalScrollBar();
    bar->setRange(0, maximum);

    if (maximumOffset <= INT_MAX) {
        bar->setPageStep(std::max(1, viewport()->height()));
        bar->setSingleStep(std::max(1, defaultHeight / 3));
    } else {
        const long double scale = static_cast<long double>(maximum)
            / static_cast<long double>(maximumOffset);
        bar->setPageStep(std::max(1,
            static_cast<int>(std::llround(viewport()->height() * scale))));
        bar->setSingleStep(std::max(1,
            static_cast<int>(std::llround((defaultHeight / 3.0L) * scale))));
    }
    bar->setValue(scrollValueForContentOffset(preservedOffset));
}

void LongListWidget::onScrollValueChanged()
{
    layoutMaterialized();

    const bool userScroll =
        wheelInProgress || verticalScrollBar()->isSliderDown() || userScrollActionPending;
    const VisibilityChangeReason visibilityReason = userScroll
        ? VisibilityChangeReason::UserScroll
        : ((internalScrollChange || scrollAnimationApplying)
               ? VisibilityChangeReason::ProgrammaticScroll
               : VisibilityChangeReason::LayoutChange);
    emitRangeChanges(visibilityReason);

    if (internalScrollChange || committingGeometry || synchronizing) {
        return;
    }
    if (verticalScrollBar()->isSliderDown()) {
        return;
    }
    scheduleViewportSyncIfNeeded(RequestReason::Scroll);
}

void LongListWidget::onSliderMoved(int value)
{
    // This path is used by an interactive thumb move and by the explicit
    // SliderMove action generated for an absolute groove click. Either gesture
    // gives the user authority over the viewport before sparse data is loaded.
    releaseViewportLock(true);

    const int target = logicalTargetForScrollValue(value);
    if (target < 0) {
        return;
    }

    // sliderMoved()/actionTriggered(SliderMove) can arrive before Qt copies the
    // new sliderPosition into value. Test availability against the requested
    // position itself; using the current viewport here makes a jump from a
    // resident window into sparse data look falsely ready and suppresses seek.
    const Range viewportDesired = desiredRangeForViewport(value);
    bool viewportBodiesReady = viewportDesired.isValid();
    if (viewportBodiesReady) {
        for (int index = viewportDesired.first; index <= viewportDesired.last; ++index) {
            if (!available.testBit(index)) {
                viewportBodiesReady = false;
                break;
            }
        }
    }
    if (materialized.contains(target) || viewportBodiesReady) {
        clearSeek();
        scheduleViewportSyncIfNeeded(RequestReason::Scroll, value);
        return;
    }

    if (!seekActive || target != seekTarget) {
        ++seekGeneration;
        seekTarget = target;
        pendingRequest.fill(false);
        observedAvailabilityRevision = availabilityRevision;
    }
    seekActive = true;
    seekTimer.start(seekDebounceInterval);
}

void LongListWidget::activateSeekTarget()
{
    if (!seekActive || seekTarget < 0) {
        return;
    }
    scheduleSync(RequestReason::Seek);
}

int LongListWidget::logicalTargetForScrollValue(int value) const
{
    if (logicalCount <= 0) {
        return -1;
    }

    // Scrollbar values represent the viewport's top content offset, not a post
    // ordinal. Map the centre of that viewport through the height index so the
    // bottom scrollbar position targets the centre of the last visible rows
    // rather than always targeting the final logical item.
    const qint64 top = contentOffsetForScrollValue(value);
    const qint64 center = std::min<qint64>(
        std::max<qint64>(0, heights.totalHeight() - 1),
        top + std::max(0, viewport()->height() / 2));
    return heights.indexAtPixel(center);
}

void LongListWidget::clearSeek()
{
    if (seekActive || seekTarget >= 0) {
        ++seekGeneration;
    }
    seekActive = false;
    seekTarget = -1;
    seekTimer.stop();
}

void LongListWidget::emitRangeChanges(VisibilityChangeReason reason)
{
    const Range visible = visibleRange();
    if (!sameRange(visible, lastVisibleRange)) {
        lastVisibleRange = visible;
        emit visibleRangeChanged(visible.first, visible.last);
    }

    const Range concrete = materializedRange();
    if (!sameRange(concrete, lastMaterializedRange)) {
        lastMaterializedRange = concrete;
        emit materializedRangeChanged(concrete.first, concrete.last);
    }

    updateItemVisibilities(reason);
}

void LongListWidget::updateItemVisibilities(VisibilityChangeReason reason)
{
    QSet<int> indices;
    for (auto it = lastItemVisibilities.cbegin(); it != lastItemVisibilities.cend(); ++it) {
        indices.insert(it.key());
    }
    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        indices.insert(it.key());
    }

    QVector<int> ordered;
    ordered.reserve(indices.size());
    for (int index : std::as_const(indices)) {
        ordered.push_back(index);
    }
    std::sort(ordered.begin(), ordered.end());

    QHash<int, ItemVisibilities> current;
    for (int index : std::as_const(ordered)) {
        const ItemVisibilities visibility = itemVisibility(index);
        const ItemVisibilities previous = lastItemVisibilities.value(index);
        if (visibility != previous) {
            emit itemVisibilityChanged(index, visibility, reason);
        }
        if (visibility != ItemVisibilities()) {
            current.insert(index, visibility);
        }
    }
    lastItemVisibilities = std::move(current);
}

} // namespace Mattermost
