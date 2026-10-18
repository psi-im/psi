/*
 * Copyright (c) 2026 Sergei Ilinykh
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This license applies to both LongListWidget.h and LongListWidget.cpp.
 */

#pragma once

#include <QAbstractScrollArea>
#include <QBitArray>
#include <QColor>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>

class QPaintEvent;

namespace Mattermost {

/**
 * A virtualized widget list for very long, partially available sequences.
 *
 * LongListWidget owns all scroll geometry, materialization and viewport anchoring.
 * There are no placeholder/gap widgets: unavailable or unmaterialized items are
 * represented only by their estimated heights in the internal height index.
 *
 * Subclasses provide concrete item widgets through createItemWidget(). Data
 * availability is reported independently with setRangeAvailable(). When a range
 * required by the viewport is unavailable, rangeRequested() is emitted.
 */
class LongListWidget : public QAbstractScrollArea
{
    Q_OBJECT
    Q_PROPERTY(bool hoverHighlightEnabled
               READ isHoverHighlightEnabled
               WRITE setHoverHighlightEnabled)
public:
    enum class RequestReason {
        Initial,
        Scroll,
        Seek,
        EnsureVisible,
    };
    Q_ENUM(RequestReason)

    enum class Alignment {
        EnsureVisible,
        Top,
        Center,
        Bottom,
    };
    enum class SemanticAnchorPoint {
        ItemCenter,
        ItemBottom,
    };
    Q_ENUM(Alignment)

    enum class InsertViewportPolicy {
        PreserveIntent,
        PreserveVisibleContent,
    };
    Q_ENUM(InsertViewportPolicy)

    enum class ItemVisibility {
        None   = 0,
        Body   = 1 << 0,
        Top    = 1 << 1,
        Bottom = 1 << 2,
    };
    Q_ENUM(ItemVisibility)
    Q_DECLARE_FLAGS(ItemVisibilities, ItemVisibility)
    Q_FLAG(ItemVisibilities)

    enum class VisibilityChangeReason {
        UserScroll,
        ProgrammaticScroll,
        LayoutChange,
    };
    Q_ENUM(VisibilityChangeReason)

    struct Range {
        int first = -1;
        int last = -1;

        bool isValid() const { return first >= 0 && last >= first; }
        int count() const { return isValid() ? last - first + 1 : 0; }
        bool contains(int index) const
        {
            return isValid() && index >= first && index <= last;
        }
    };

    explicit LongListWidget(QWidget* parent = nullptr);
    ~LongListWidget() override;

    int itemCount() const { return logicalCount; }
    void setItemCount(int count);

    /**
     * Insert real logical items.
     *
     * PreserveIntent retains sticky-bottom when it is current viewport intent.
     * PreserveVisibleContent converts sticky-bottom into the concrete old item
     * anchor so an appended row may settle below the viewport before an
     * explicit follow operation.
     */
    void insertItems(int first,
                     int count,
                     InsertViewportPolicy viewportPolicy = InsertViewportPolicy::PreserveIntent);

    /** Remove logical items and shift later identities left without losing the viewport anchor. */
    void removeItems(int first, int count);

    /** Replace one logical identity in-place in one frozen geometry transaction. */
    void replaceItem(int index);

    int defaultItemHeight() const { return defaultHeight; }
    void setDefaultItemHeight(int height);

    int materializationLimit() const { return maxMaterializedItems; }
    void setMaterializationLimit(int count);

    int requestBlockSize() const { return blockSize; }
    void setRequestBlockSize(int count);

    /** Optional extra screen buffer beyond the adaptive half-screen row margin. */
    int prefetchScreens() const { return bufferScreens; }
    void setPrefetchScreens(int screens);

    int seekDebounceMs() const { return seekDebounceInterval; }
    void setSeekDebounceMs(int milliseconds);

    /** Generic row hover feedback is enabled by default and can be disabled by subclasses/users. */
    bool isHoverHighlightEnabled() const { return hoverHighlightEnabled; }
    void setHoverHighlightEnabled(bool enabled);

    /**
     * Opaque surface color visually equivalent to the row hover overlay painted
     * by this list over its viewport background. Floating row UI should use this
     * instead of duplicating hover alpha/palette rules.
     */
    QColor hoverHighlightSurfaceColor() const;

    void setRangeAvailable(int first, int last, bool available = true);
    bool isItemAvailable(int index) const;

    /**
     * Complete one source/transport attempt for a logical range.
     *
     * Concrete off-target availability may wake the current viewport once after
     * pending suppression is released. A no-progress completion never polls the
     * source again on its own: semantic cursor convergence belongs to the source,
     * while a network/source failure waits for new progress or a user gesture.
     */
    void finishRangeRequest(int first, int last);
    void resolveSeekTarget(int index, quint64 generation);

    /** Re-measure already materialized items after their presentation changed in-place. */
    void itemsChanged(int first, int last);

    /**
     * Commit an explicit child geometry change synchronously. Use this only for
     * a semantic widget signal that guarantees its size hints are already final;
     * generic Qt LayoutRequest/Resize hints remain coalesced through itemsChanged().
     */
    void commitItemGeometryNow(int index);

    /**
     * Reconcile an identity-to-index mapping change without rebuilding surviving
     * widgets. Subclasses that support arbitrary remaps provide stable semantic
     * identity through itemIdentity()/indexOfItemIdentity(). A widget is kept
     * whenever the same identity still exists and remains available; only its
     * logical index and geometry change.
     */
    void reconcileItemLayout(int first, int last);

    QWidget* itemWidget(int index) const;
    QVector<int> materializedIndices() const;
    int materializedCount() const { return materialized.size(); }

    Range visibleRange() const;
    Range materializedRange() const;

    qint64 contentHeight() const;
    int indexAtViewportPosition(int viewportY) const;
    int viewportCenterIndex() const;
    // Relative to the viewport's same reference point, normalized by height.
    // May be negative, notably when the item's centre lies above the viewport.
    bool captureSemanticAnchorFraction(int index,
                                      SemanticAnchorPoint point,
                                      qreal& fraction) const;
    ItemVisibilities itemVisibility(int index) const;
    bool isAtEnd() const
    {
        return maximumContentOffset() == contentOffset();
    }

    void scrollToIndex(int index, Alignment alignment = Alignment::EnsureVisible);
    void scrollToEnd();
    void scrollToEndAnimated(int durationMs = 180);

    /**
     * Keep a logical item at the requested semantic alignment while its own
     * height settles. Geometry changes elsewhere preserve the current screen Y;
     * a target-height change re-applies the requested alignment. Centering an
     * item taller than the viewport is defined as top alignment. Direct user
     * scrolling releases the lock. A zero quiet period keeps it until that
     * user gesture.
     */
    bool lockViewportToItem(int index,
                            Alignment alignment = Alignment::Center,
                            int quietPeriodMs = 2000);
    // Restore a specific item center relative to viewport center, or item
    // bottom relative to viewport bottom, even across changes in item height.
    bool lockViewportToSemanticAnchor(int index,
                                      SemanticAnchorPoint point,
                                      qreal fraction,
                                      int quietPeriodMs = 0);

    /**
     * Change the logical index represented by an active viewport lock without
     * changing its screen position. Used when source identity is remapped from
     * an estimated slot to an authoritative slot.
     */
    bool remapViewportLockedItem(int index);

    void clearViewportLock();
    bool hasViewportLock() const { return viewportLock.index >= 0; }

signals:
    /**
     * Request logical items. first/last are inclusive. generation changes for
     * every new random-seek target; ordinary scroll requests use generation 0.
     */
    void rangeRequested(int first,
                        int last,
                        Mattermost::LongListWidget::RequestReason reason,
                        quint64 generation);
    /** Emitted once for every rangeRequested() after the source completes it. */
    void rangeRequestFinished(int first, int last);
    void visibleRangeChanged(int first, int last);
    void materializedRangeChanged(int first, int last);

    /** Emitted when the same top-level item hover used by the row highlight changes. */
    void hoveredItemChanged(int previousIndex, int currentIndex);

    void itemVisibilityChanged(
        int index,
        Mattermost::LongListWidget::ItemVisibilities visibility,
        Mattermost::LongListWidget::VisibilityChangeReason reason);

    /** Direct user scrolling took ownership of the viewport. */
    void userScrollStarted();

    /** Emitted when a viewport lock is released by timeout, user input or caller. */
    void viewportLockReleased();

protected:
    /** Return a new widget for an available logical item. Ownership is transferred. */
    virtual QWidget* createItemWidget(int index) = 0;

    /**
     * Stable semantic identity carried by a concrete widget. Empty means that
     * this list does not support arbitrary identity remapping; insert/remove
     * operations still preserve widgets by their shifted logical index.
     */
    virtual QString itemIdentity(const QWidget* widget) const
    {
        Q_UNUSED(widget)
        return {};
    }

    /** Return the current logical index of a stable semantic identity. */
    virtual int indexOfItemIdentity(const QString& identity) const
    {
        Q_UNUSED(identity)
        return -1;
    }

    /** Current model-side availability used while reconciling a remapped span. */
    virtual bool isModelItemAvailable(int index) const
    {
        return isItemAvailable(index);
    }

    /** Default destruction policy for an evicted materialized widget. */
    virtual void destroyItemWidget(int index, QWidget* widget);

    /** Override when some unmeasured items have a better per-index estimate. */
    virtual int estimatedItemHeight(int index) const;

    /**
     * Keep hover paint on a materialized item while an external overlay
     * logically belonging to that row owns the pointer. This affects only the
     * painted highlight; hoveredItemChanged continues to report physical row
     * enter/leave events.
     */
    void setHoverHighlightOverride(QWidget* widget);
    QWidget* hoverHighlightOverrideWidget() const
    {
        return hoverHighlightOverride.data();
    }

    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class HeightIndex
    {
    public:
        void reset(int count, int height);
        void resize(int count, int height);
        void insert(int first, int count, int height);
        void remove(int first, int count);
        int size() const { return static_cast<int>(values.size()); }
        int value(int index) const;
        void setValue(int index, int value);
        qint64 prefixHeight(int count) const;
        qint64 totalHeight() const { return prefixHeight(static_cast<int>(values.size())); }
        int indexAtPixel(qint64 pixel) const;

    private:
        void add(int index, qint64 delta);
        void rebuildFenwick();

        QVector<int> values;
        QVector<qint64> fenwick;
    };

    struct ViewAnchor {
        enum Kind {
            Invalid,
            Item,
            Bottom,
        } kind = Invalid;
        int index = -1;
        qint64 offsetInsideItem = 0;
    };

    struct ViewportLock {
        int index = -1;
        long double itemTopFraction = 0.0L;
        int itemHeight = 0;
        Alignment alignment = Alignment::EnsureVisible;
        bool explicitSemanticAnchor = false;
        SemanticAnchorPoint semanticPoint = SemanticAnchorPoint::ItemCenter;
        long double semanticFraction = 0.0L;
        int quietPeriodMs = 0;
    };

    void scheduleSync(RequestReason reason);
    void scheduleViewportSyncIfNeeded(RequestReason reason, int scrollValue = -1);
    void synchronize();
    void synchronizeRange(const Range& desired,
                          RequestReason reason,
                          quint64 generation,
                          bool centerSeekTarget);

    Range desiredRangeForViewport(int scrollValue = -1) const;
    Range desiredRangeForSeek() const;
    Range boundedAround(int center, int count) const;
    Range clampToBudget(const Range& range, int preferredCenter) const;

    void materializeAvailable(const Range& range);
    void evictOutside(const Range& keepRange, int preferredCenter);
    void layoutMaterialized();
    /** Measure one widget at viewport width without mutating the logical HeightIndex. */
    int naturalWidgetHeight(QWidget* widget);
    /** Measure one materialized row; returns true only if its logical height changed. */
    bool measureWidget(int index, QWidget* widget);
    void commitReplacement(QWidget* previous,
                           QWidget* replacement,
                           const QString& replacementIdentity,
                           int fallbackIndex);
    void stageReplacement(QWidget* previous,
                          QWidget* replacement,
                          const QString& replacementIdentity,
                          int fallbackIndex,
                          int observedHeight);
    void scheduleGeometryCommit(int index = -1);
    void commitGeometry(bool heightIndexChanged = false);

    void requestMissing(const Range& desired,
                        RequestReason reason,
                        quint64 generation);
    void clearPendingRequest(int first, int last);
    bool hasMissingItems(const Range& range) const;
    bool hasMissingVisibleItems() const;

    ViewAnchor captureAnchor() const;
    void restoreAnchor(const ViewAnchor& anchor);
    void restoreSeekTarget();
    qint64 alignedContentOffset(int index, Alignment alignment) const;
    void captureViewportLockFraction();
    void restoreViewportLock();
    void touchViewportLock();
    void releaseViewportLock(bool notify);
    void noteUserScrollStarted();
    void stopScrollAnimation();

    qint64 maximumContentOffset() const;
    qint64 contentOffset() const;
    int scrollValueForContentOffset(qint64 offset) const;
    qint64 contentOffsetForScrollValue(int value) const;
    void updateScrollBarRange(qint64 preservedOffset);

    void onScrollValueChanged();
    void onSliderMoved(int value);
    void activateSeekTarget();
    int logicalTargetForScrollValue(int value) const;
    void clearSeek();

    void emitRangeChanges(VisibilityChangeReason reason = VisibilityChangeReason::LayoutChange);
    void updateItemVisibilities(VisibilityChangeReason reason);

    int logicalCount = 0;
    int defaultHeight = 96;
    int maxMaterializedItems = 200;
    int blockSize = 10;
    int bufferScreens = 0;
    int seekDebounceInterval = 100;

    HeightIndex heights;
    QBitArray measured;
    QBitArray available;
    QBitArray pendingRequest;

    QHash<int, QWidget*> materialized;
    QHash<QObject*, int> widgetIndexes;
    QSet<int> dirtyGeometry;
    QPointer<QWidget> hoveredWidget;
    QPointer<QWidget> hoverHighlightOverride;

    QTimer syncTimer;
    QTimer geometryTimer;
    QTimer seekTimer;
    QTimer viewportLockTimer;
    QVariantAnimation scrollAnimation;
    RequestReason pendingSyncReason = RequestReason::Initial;

    bool synchronizing = false;
    bool committingGeometry = false;
    bool internalScrollChange = false;
    bool wheelInProgress = false;
    bool userScrollActionPending = false;
    bool scrollAnimationApplying = false;
    bool hoverHighlightEnabled = true;

    quint64 seekGeneration = 0;
    int seekTarget = -1;
    bool seekActive = false;
    int scrollAnimationStartValue = 0;

    quint64 availabilityRevision = 0;
    quint64 observedAvailabilityRevision = 0;

    ViewportLock viewportLock;

    Range lastVisibleRange;
    Range lastMaterializedRange;
    Range lastSynchronizedViewportRange;
    QHash<int, ItemVisibilities> lastItemVisibilities;
};

} // namespace Mattermost

Q_DECLARE_OPERATORS_FOR_FLAGS(Mattermost::LongListWidget::ItemVisibilities)
