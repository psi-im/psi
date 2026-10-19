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
    static void install(QApplication& application);

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct State;

    explicit OverlayScrollBarManager(QApplication& application);

    void registerArea(QAbstractScrollArea* area);
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
