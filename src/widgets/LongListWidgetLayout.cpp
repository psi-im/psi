/*
 * Copyright (c) 2026 Sergei Ilinykh
 * SPDX-License-Identifier: MIT
 */

#include "LongListWidget.h"

#include <algorithm>
#include <utility>

#include <QScrollBar>
#include <QSignalBlocker>

namespace Mattermost {

void LongListWidget::reconcileItemLayout(int first, int last)
{
    if (logicalCount <= 0) {
        return;
    }

    first = std::max(0, first);
    last = std::min(logicalCount - 1, last);
    if (last < first) {
        return;
    }

    // A few legacy producers still route an ordinary one-row data update through
    // the structural notification while the migration away from itemsChanged is
    // being completed. If the concrete semantic row is demonstrably still at the
    // same index with the same availability, there is no layout work to do. The
    // presentation-specific path re-measures that widget independently. Besides
    // avoiding needless scrollbar restoration, this makes such notifications
    // harmless during wheel scrolling.
    if (first == last) {
        if (QWidget* widget = materialized.value(first, nullptr)) {
            const QString identity = itemIdentity(widget);
            if (!identity.isEmpty()
                && indexOfItemIdentity(identity) == first
                && isModelItemAvailable(first) == isItemAvailable(first)) {
                return;
            }
        }
    }

    struct WidgetState {
        QWidget* widget = nullptr;
        QString identity;
        int oldIndex = -1;
        int newIndex = -1;
        int height = 0;
        bool measured = false;
        bool dirty = false;
        bool keep = false;
    };

    const ViewAnchor oldAnchor = captureAnchor();
    ViewAnchor anchor = oldAnchor;
    const qint64 oldOffset = contentOffset();

    const auto identityAtIndex = [this](int index) {
        QWidget* widget = materialized.value(index, nullptr);
        return widget ? itemIdentity(widget) : QString();
    };

    const QString anchorIdentity = oldAnchor.kind == ViewAnchor::Item
        ? identityAtIndex(oldAnchor.index) : QString();
    const QString lockIdentity = hasViewportLock()
        ? identityAtIndex(viewportLock.index) : QString();
    const QString seekIdentity = seekActive && seekTarget >= 0
        ? identityAtIndex(seekTarget) : QString();

    QVector<WidgetState> states;
    states.reserve(materialized.size());
    QSet<int> touched;
    for (int index = first; index <= last; ++index) {
        touched.insert(index);
    }

    for (auto it = materialized.cbegin(); it != materialized.cend(); ++it) {
        WidgetState state;
        state.widget = it.value();
        state.oldIndex = it.key();
        state.identity = itemIdentity(state.widget);
        state.height = heights.value(state.oldIndex);
        state.measured = measured.testBit(state.oldIndex);
        state.dirty = dirtyGeometry.contains(state.oldIndex);

        // Lists that never perform arbitrary remaps may leave semantic identity
        // unsupported. Keeping the existing logical index preserves the normal
        // insert/remove behavior; callers that use reconcileItemLayout() should
        // provide stable identity for every concrete widget.
        state.newIndex = state.identity.isEmpty()
            ? state.oldIndex : indexOfItemIdentity(state.identity);
        state.keep = state.newIndex >= 0 && state.newIndex < logicalCount
            && isModelItemAvailable(state.newIndex);

        touched.insert(state.oldIndex);
        if (state.newIndex >= 0 && state.newIndex < logicalCount) {
            touched.insert(state.newIndex);
        }
        states.push_back(std::move(state));
    }

    // Height measurements belong to semantic items, not logical slots. Reset
    // every touched slot first, then restore the exact measurements alongside
    // widgets that survive the identity remap.
    for (int index : std::as_const(touched)) {
        available.setBit(index, isModelItemAvailable(index));
        pendingRequest.clearBit(index);
        if (measured.testBit(index)) {
            measured.clearBit(index);
            heights.setValue(index, estimatedItemHeight(index));
        }
    }

    materialized.clear();
    widgetIndexes.clear();
    dirtyGeometry.clear();

    for (WidgetState& state : states) {
        if (!state.keep || materialized.contains(state.newIndex)) {
            if (state.widget) {
                if (hoveredWidget == state.widget) {
                    hoveredWidget.clear();
                }
                state.widget->removeEventFilter(this);
                destroyItemWidget(state.oldIndex, state.widget);
            }
            continue;
        }

        materialized.insert(state.newIndex, state.widget);
        widgetIndexes.insert(state.widget, state.newIndex);
        available.setBit(state.newIndex, true);
        if (state.measured) {
            heights.setValue(state.newIndex, state.height);
            measured.setBit(state.newIndex, true);
        }
        if (state.dirty) {
            dirtyGeometry.insert(state.newIndex);
        }
    }

    if (!anchorIdentity.isEmpty()) {
        const int newAnchorIndex = indexOfItemIdentity(anchorIdentity);
        if (newAnchorIndex >= 0 && newAnchorIndex < logicalCount) {
            anchor.index = newAnchorIndex;
        }
    }

    if (!lockIdentity.isEmpty()) {
        const int newLockIndex = indexOfItemIdentity(lockIdentity);
        if (newLockIndex >= 0 && newLockIndex < logicalCount) {
            viewportLock.index = newLockIndex;
        } else {
            // A structural remap is not a user gesture. Drop only the physical
            // lock and let a semantic owner decide whether to reacquire it.
            releaseViewportLock(false);
        }
    }

    if (!seekIdentity.isEmpty()) {
        const int newSeekIndex = indexOfItemIdentity(seekIdentity);
        if (newSeekIndex >= 0 && newSeekIndex < logicalCount) {
            seekTarget = newSeekIndex;
        }
    }

    // Treat a remap as one geometry transaction. New mappings, carried height
    // measurements and the semantic anchor are committed before the scrollbar is
    // touched, so wheel input never observes the old widget disappearing through
    // an intermediate estimated-height state.
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

} // namespace Mattermost
