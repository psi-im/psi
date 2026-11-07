/*
 * Copyright 2026 Sergei Ilinykh
 *
 * This file is part of Mattermost-QT.
 *
 * Mattermost-QT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QHash>
#include <QObject>

class QApplication;
class QAbstractScrollArea;
class QEvent;
class QWidget;

namespace Mattermost {

/**
 * Replaces scroll-area chrome with thin translucent scroll bars layered on top
 * of the viewport. The original QAbstractScrollArea scroll bars remain the
 * authoritative scroll model, so existing wheel, keyboard and programmatic
 * scrolling behavior is unchanged.
 */
class OverlayScrollBarManager final : public QObject
{
public:
    /**
     * Explicitly enable the overlay on this scroll area. Safe to call more
     * than once; attaching never changes unrelated QAbstractScrollArea widgets.
     * The manager is created lazily and owned by QApplication.
     */
    static void attach(QAbstractScrollArea& area);

    /**
     * Temporarily suppress or restore a managed overlay, e.g. horizontal
     * scrolling while text is wrapped. Call before or after attach(); the
     * original scrollbars stay hidden. Axes whose Qt policy is AlwaysOff at
     * attach time are not managed. Do not change the area's Qt scrollbar
     * policies after attaching.
     */
    static void setScrollBarEnabled(QAbstractScrollArea& area,
                                    Qt::Orientation orientation, bool enabled);

    /**
     * Briefly reveal the overlay for an already-populated scroll area.
     * Returns true when the area currently has scrollable content.
     */
    static bool pulse(QAbstractScrollArea& area);

    /**
     * Controls the top-edge navigation affordance for one scroll area.
     * The bottom-edge affordance remains available whenever the area is away
     * from its maximum. Main conversation timelines disable the top action,
     * while threads and ordinary lists keep it.
     */
    static void setScrollToStartButtonEnabled(QAbstractScrollArea& area, bool enabled);

    /**
     * Enable or disable both edge-jump affordances while keeping the overlay
     * scrollbar itself active. Compact transient views such as the emoji
     * picker want wheel/thumb scrolling without top/bottom navigation buttons.
     */
    static void setEdgeNavigationButtonsEnabled(QAbstractScrollArea& area,
                                                bool enabled);

    /**
     * Optional generic badge: set "mattermostOverlayBottomBadgeText" on the
     * scroll area to a nonempty label. The manager owns drawing/positioning;
     * the consumer decides what the label means.
     */

    /** True for an edge navigation button or its invisible exclusion zone. */
    static bool isEdgeNavigationExclusion(const QObject* object);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct State;

    explicit OverlayScrollBarManager(QApplication& application);

    void attachArea(QAbstractScrollArea& area);
    State* stateForWidget(QWidget* widget) const;
    void sync(State& state);
    void layout(State& state);
    void updatePalette(State& state);
    void reveal(State& state);
    void fade(State& state);
    void scheduleFade(State& state);
    bool containsCursor(const State& state) const;
    bool cursorOverOverlay(const State& state) const;

    QHash<QAbstractScrollArea*, State*> states;
};

} // namespace Mattermost
