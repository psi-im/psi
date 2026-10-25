/*
 * Copyright 2026 Sergei Ilinykh
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Adapted from MatterLeast's ThinSplitter for Psi.
 */
#pragma once

#include <QSplitter>

/**
 * A one-pixel splitter whose expanded mouse target overlaps only the pane
 * after the divider. The preceding pane (and its overlay scrollbar) retains
 * its full input region.
 */
class ThinSplitter final : public QSplitter
{
public:
    static constexpr int VisibleHandleExtent = 1;
    static constexpr int TrailingHitPadding = 6;

    explicit ThinSplitter(QWidget *parent = nullptr);
    ThinSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    QSplitterHandle *createHandle() override;
};
