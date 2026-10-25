/*
 * Copyright 2026 Sergei Ilinykh
 *
 * This file is part of Mattermost-QT.
 *
 * Mattermost-QT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mattermost-QT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#include "OverlayScrollBarManager.h"


#include <algorithm>
#include <cmath>

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QPalette>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

namespace Mattermost {
namespace {

// Keep the painted thumb narrow, but make its actual mouse target considerably
// more forgiving. The outside pixel remains visually empty while still
// belonging to the scrollbar, so the pointer cannot slip through at the window
// edge. The inner hit padding is roughly half of the visible thumb thickness.
constexpr int VisibleThumbThickness = 5;
constexpr int OuterHitPadding = 1;
constexpr int InnerHitPadding = (VisibleThumbThickness + 1) / 2;
constexpr int ScrollBarHitThickness = InnerHitPadding + VisibleThumbThickness + OuterHitPadding;
constexpr int ScrollBarEndInset = 1;
constexpr int FadeDelayMs = 900;
constexpr int FadeDurationMs = 240;
constexpr int RevealDurationMs = 90;
constexpr int EdgeButtonGap = 2;
constexpr qreal EdgeGlyphScale = 1.5;
constexpr qreal EdgeButtonIdleOpacity = 0.58;
constexpr qreal EdgeButtonHoverOpacity = 1.0;
constexpr char InstalledProperty[] = "mattermostOverlayScrollBarsInstalled";
constexpr char ScrollToStartEnabledProperty[] = "mattermostOverlayScrollToStartEnabled";
constexpr char EdgeButtonsEnabledProperty[] = "mattermostOverlayEdgeButtonsEnabled";
constexpr char VerticalObjectName[] = "mattermostOverlayVerticalScrollBar";
constexpr char HorizontalObjectName[] = "mattermostOverlayHorizontalScrollBar";
constexpr char ScrollToStartObjectName[] = "mattermostOverlayScrollToStartButton";
constexpr char ScrollToEndObjectName[] = "mattermostOverlayScrollToEndButton";

QString cssRgba(const QColor& color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString overlayStyleSheet(const QColor& handle)
{
    return QStringLiteral(
        "QScrollBar:vertical {"
        " background: transparent; border: 0; width: %2px; margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        " background: %1; border: 0; border-radius: 2px;"
        " min-height: 28px; margin: 1px %3px 1px %4px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        " height: 0; border: 0; background: transparent;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background: transparent;"
        "}"
        "QScrollBar:horizontal {"
        " background: transparent; border: 0; height: %2px; margin: 0;"
        "}"
        "QScrollBar::handle:horizontal {"
        " background: %1; border: 0; border-radius: 2px;"
        " min-width: 28px; margin: %4px 1px %3px 1px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        " width: 0; border: 0; background: transparent;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        " background: transparent;"
        "}")
        .arg(cssRgba(handle))
        .arg(ScrollBarHitThickness)
        .arg(OuterHitPadding)
        .arg(InnerHitPadding);
}

QScrollBar* createOverlay(QAbstractScrollArea& area,
                          Qt::Orientation orientation,
                          const char* objectName)
{
    auto* bar = new QScrollBar(orientation, &area);
    bar->setObjectName(QString::fromLatin1(objectName));
    bar->setFocusPolicy(Qt::NoFocus);
    bar->setMouseTracking(true);
    bar->hide();
    return bar;
}

bool scrollable(const QScrollBar* bar)
{
    return bar && bar->maximum() > bar->minimum();
}

bool scrollToStartButtonEnabled(const QAbstractScrollArea& area)
{
    const QVariant value = area.property(ScrollToStartEnabledProperty);
    return !value.isValid() || value.toBool();
}

bool edgeButtonsEnabled(const QAbstractScrollArea& area)
{
    const QVariant value = area.property(EdgeButtonsEnabledProperty);
    return !value.isValid() || value.toBool();
}

QFont edgeButtonFont(const QAbstractScrollArea& area)
{
    QFont font = area.font();
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(font.pointSizeF() * EdgeGlyphScale);
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(std::max(1,
            static_cast<int>(std::lround(font.pixelSize() * EdgeGlyphScale))));
    }
    return font;
}

QToolButton* createEdgeButton(QAbstractScrollArea& area,
                              const QString& text,
                              const char* objectName,
                              const QString& label)
{
    auto* button = new QToolButton(&area);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setText(text);
    button->setFont(edgeButtonFont(area));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(label);
    button->setAccessibleName(label);
    // Do not let the platform QToolButton style draw a rectangular hover frame.
    // The affordance is intentionally only the glyph; hover feedback is opacity.
    button->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; background: transparent; padding: 0; }"
        "QToolButton:hover, QToolButton:pressed {"
        " border: 0; background: transparent;"
        "}"));
    auto* opacity = new QGraphicsOpacityEffect(button);
    opacity->setOpacity(EdgeButtonIdleOpacity);
    button->setGraphicsEffect(opacity);
    button->hide();
    return button;
}

int edgeButtonExtent(const QToolButton* button)
{
    return button ? std::max(18, QFontMetrics(button->font()).height() + 4) : 0;
}

void syncBar(QScrollBar* source, QScrollBar* overlay)
{
    if (!source || !overlay) {
        return;
    }

    const QSignalBlocker blocker(overlay);
    overlay->setRange(source->minimum(), source->maximum());
    overlay->setPageStep(source->pageStep());
    overlay->setSingleStep(source->singleStep());
    overlay->setTracking(source->hasTracking());
    overlay->setInvertedAppearance(source->invertedAppearance());
    overlay->setInvertedControls(source->invertedControls());
    overlay->setValue(source->value());
}

void animateOpacity(QGraphicsOpacityEffect* effect,
                    QPropertyAnimation* animation,
                    qreal target,
                    int duration)
{
    if (!effect || !animation) {
        return;
    }
    // Cancel the opposite in-flight animation even when its first frame still
    // equals the new target (e.g. leaving immediately after a reveal starts).
    animation->stop();
    if (std::abs(effect->opacity() - target) < 0.01) {
        effect->setOpacity(target);
        return;
    }
    animation->setDuration(duration);
    animation->setStartValue(effect->opacity());
    animation->setEndValue(target);
    animation->start();
}

} // namespace

struct OverlayScrollBarManager::State {
    QAbstractScrollArea* area = nullptr;
    QScrollBar* sourceVertical = nullptr;
    QScrollBar* sourceHorizontal = nullptr;
    QScrollBar* overlayVertical = nullptr;
    QScrollBar* overlayHorizontal = nullptr;
    QToolButton* scrollToStartButton = nullptr;
    QToolButton* scrollToEndButton = nullptr;
    QGraphicsOpacityEffect* verticalOpacity = nullptr;
    QGraphicsOpacityEffect* horizontalOpacity = nullptr;
    QPropertyAnimation* verticalAnimation = nullptr;
    QPropertyAnimation* horizontalAnimation = nullptr;
    QTimer* fadeTimer = nullptr;
    bool verticalEnabled = false;
    bool horizontalEnabled = false;
    bool cursorInside = false;
};

void OverlayScrollBarManager::install(QApplication& application)
{
    if (application.property(InstalledProperty).toBool()) {
        return;
    }
    application.setProperty(InstalledProperty, true);
    new OverlayScrollBarManager(application);
}

void OverlayScrollBarManager::setScrollToStartButtonEnabled(QAbstractScrollArea& area,
                                                             bool enabled)
{
    area.setProperty(ScrollToStartEnabledProperty, enabled);
}

void OverlayScrollBarManager::setEdgeNavigationButtonsEnabled(
    QAbstractScrollArea& area,
    bool enabled)
{
    area.setProperty(EdgeButtonsEnabledProperty, enabled);
}

OverlayScrollBarManager::OverlayScrollBarManager(QApplication& application)
    : QObject(&application)
{
    application.installEventFilter(this);

    // Usually the manager is installed before any windows are constructed, but
    // keep late installation deterministic for tests and embedded use.
    for (QWidget* widget : QApplication::allWidgets()) {
        if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
            registerArea(area);
        }
    }
}

void OverlayScrollBarManager::registerArea(QAbstractScrollArea* area)
{
    // Psi explicitly opts in native timelines and the two contact rosters.
    // Keep unrelated legacy text views and embedded editors unchanged.
    if (area && !area->property("psiOverlayScrollBar").toBool())
        return;
    if (!area || states.contains(area)) {
        return;
    }

    auto* state = new State;
    state->area = area;
    state->sourceVertical = area->verticalScrollBar();
    state->sourceHorizontal = area->horizontalScrollBar();
    state->verticalEnabled = area->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;
    state->horizontalEnabled = area->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;

    states.insert(area, state);

    const auto setupOverlay = [](QScrollBar* bar,
                                 QGraphicsOpacityEffect*& effect,
                                 QPropertyAnimation*& animation) {
        if (!bar) {
            return;
        }
        effect = new QGraphicsOpacityEffect(bar);
        effect->setOpacity(0.0);
        bar->setGraphicsEffect(effect);

        animation = new QPropertyAnimation(effect, QByteArrayLiteral("opacity"), bar);
        animation->setEasingCurve(QEasingCurve::OutCubic);
    };

    if (state->verticalEnabled) {
        state->overlayVertical = createOverlay(*area, Qt::Vertical, VerticalObjectName);
        setupOverlay(state->overlayVertical, state->verticalOpacity, state->verticalAnimation);
        state->scrollToStartButton = createEdgeButton(
            *area,
            QString::fromUtf8("\xE2\xAC\x86\xEF\xB8\x8F"),
            ScrollToStartObjectName,
            tr("Scroll to top"));
        state->scrollToEndButton = createEdgeButton(
            *area,
            QString::fromUtf8("\xE2\xAC\x87\xEF\xB8\x8F"),
            ScrollToEndObjectName,
            tr("Scroll to bottom"));
        connect(state->scrollToStartButton, &QToolButton::clicked, area,
                [source = state->sourceVertical] {
            if (source) {
                source->triggerAction(QAbstractSlider::SliderToMinimum);
            }
        });
        connect(state->scrollToEndButton, &QToolButton::clicked, area,
                [source = state->sourceVertical] {
            if (source) {
                source->triggerAction(QAbstractSlider::SliderToMaximum);
            }
        });
        area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    if (state->horizontalEnabled) {
        state->overlayHorizontal = createOverlay(*area, Qt::Horizontal, HorizontalObjectName);
        setupOverlay(state->overlayHorizontal, state->horizontalOpacity, state->horizontalAnimation);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    state->fadeTimer = new QTimer(area);
    state->fadeTimer->setSingleShot(true);
    state->fadeTimer->setInterval(FadeDelayMs);

    const auto connectSource = [this, state](QScrollBar* source, QScrollBar* overlay) {
        if (!source || !overlay) {
            return;
        }
        connect(source, &QScrollBar::rangeChanged, this, [this, state](int, int) {
            sync(*state);
            layout(*state);
            if (cursorOverOverlay(*state)) {
                scheduleFade(*state);
                reveal(*state);
            }
        });
        connect(source, &QScrollBar::valueChanged, this, [this, state](int) {
            sync(*state);
            reveal(*state);
            scheduleFade(*state);
        });

        // Mirror the overlay's user interaction onto the hidden source scrollbar
        // instead of copying only its value. LongListWidget intentionally uses
        // sliderMoved/sliderReleased to distinguish a thumb seek from ordinary
        // scrolling and to debounce materialization while the thumb is dragged.
        // Keeping source.sliderDown in sync also preserves QAbstractSlider's
        // tracking semantics on both Qt 5 and Qt 6.
        connect(overlay, &QScrollBar::sliderPressed, this, [this, state, source] {
            source->setSliderDown(true);
            if (state->fadeTimer) {
                state->fadeTimer->stop();
            }
            reveal(*state);
        });
        connect(overlay, &QScrollBar::sliderMoved, this, [source](int position) {
            source->setSliderPosition(position);
        });
        connect(overlay, &QScrollBar::sliderReleased, this, [this, state, source] {
            source->setSliderDown(false);
            if (cursorOverOverlay(*state))
                reveal(*state);
            scheduleFade(*state);
        });

        // Page-step, wheel and keyboard actions over the overlay do not put the
        // thumb down, but they still need to reach the authoritative scrollbar
        // as real slider actions so its observers receive actionTriggered and
        // valueChanged in the usual order.
        connect(overlay, &QScrollBar::actionTriggered, this,
                [source, overlay](int) {
            if (overlay->isSliderDown()) {
                return;
            }
            source->setSliderPosition(overlay->sliderPosition());
            if (!source->hasTracking()) {
                source->triggerAction(QAbstractSlider::SliderMove);
            }
        });
    };

    connectSource(state->sourceVertical, state->overlayVertical);
    connectSource(state->sourceHorizontal, state->overlayHorizontal);

    connect(state->fadeTimer, &QTimer::timeout, area, [this, state] {
        const bool dragging = (state->overlayVertical && state->overlayVertical->isSliderDown())
            || (state->overlayHorizontal && state->overlayHorizontal->isSliderDown());
        if (dragging || cursorOverOverlay(*state)) {
            reveal(*state);
            scheduleFade(*state); // Recheck even if Qt loses the eventual Leave event.
            return;
        }
        fade(*state);
    });

    connect(area, &QObject::destroyed, this, [this, area] {
        delete states.take(area);
    });

    sync(*state);
    updatePalette(*state);
    layout(*state);
    state->cursorInside = containsCursor(*state);
    if (state->cursorInside) {
        reveal(*state);
        if (!cursorOverOverlay(*state)) {
            scheduleFade(*state);
        }
    }
}

OverlayScrollBarManager::State* OverlayScrollBarManager::stateForWidget(QWidget* widget) const
{
    for (QWidget* current = widget; current; current = current->parentWidget()) {
        if (auto* area = qobject_cast<QAbstractScrollArea*>(current)) {
            const auto it = states.constFind(area);
            if (it != states.cend()) {
                return it.value();
            }
        }
    }
    return nullptr;
}

void OverlayScrollBarManager::sync(State& state)
{
    syncBar(state.sourceVertical, state.overlayVertical);
    syncBar(state.sourceHorizontal, state.overlayHorizontal);

    if (state.overlayVertical) {
        if (state.verticalEnabled && scrollable(state.sourceVertical)) {
            state.overlayVertical->show();
        } else {
            state.overlayVertical->hide();
        }
    }
    if (state.overlayHorizontal) {
        if (state.horizontalEnabled && scrollable(state.sourceHorizontal)) {
            state.overlayHorizontal->show();
        } else {
            state.overlayHorizontal->hide();
        }
    }

    const bool verticalScrollable =
        state.verticalEnabled && scrollable(state.sourceVertical);
    if (state.scrollToStartButton) {
        const bool awayFromStart =
            state.sourceVertical->value() > state.sourceVertical->minimum();
        state.scrollToStartButton->setVisible(
            verticalScrollable
            && edgeButtonsEnabled(*state.area)
            && scrollToStartButtonEnabled(*state.area)
            && awayFromStart);
    }
    if (state.scrollToEndButton) {
        const bool awayFromEnd =
            state.sourceVertical->value() < state.sourceVertical->maximum();
        state.scrollToEndButton->setVisible(
            verticalScrollable
            && edgeButtonsEnabled(*state.area)
            && awayFromEnd);
    }
}

void OverlayScrollBarManager::layout(State& state)
{
    if (!state.area || !state.area->viewport()) {
        return;
    }

    const QRect viewportRect = state.area->viewport()->geometry();
    const bool verticalScrollable = state.verticalEnabled && scrollable(state.sourceVertical);
    const bool horizontalScrollable = state.horizontalEnabled && scrollable(state.sourceHorizontal);

    if (state.overlayVertical) {
        const int bottomCut = horizontalScrollable ? ScrollBarHitThickness : 0;
        const int height = std::max(0, viewportRect.height() - 2 * ScrollBarEndInset - bottomCut);
        const int x = viewportRect.right() - ScrollBarHitThickness + 1;
        state.overlayVertical->setGeometry(x,
                                           viewportRect.top() + ScrollBarEndInset,
                                           ScrollBarHitThickness,
                                           height);
        state.overlayVertical->raise();
        if (!verticalScrollable) {
            state.overlayVertical->hide();
        }
    }

    if (state.overlayHorizontal) {
        const int rightCut = verticalScrollable ? ScrollBarHitThickness : 0;
        const int width = std::max(0, viewportRect.width() - 2 * ScrollBarEndInset - rightCut);
        const int y = viewportRect.bottom() - ScrollBarHitThickness + 1;
        state.overlayHorizontal->setGeometry(viewportRect.left() + ScrollBarEndInset,
                                             y,
                                             width,
                                             ScrollBarHitThickness);
        state.overlayHorizontal->raise();
        if (!horizontalScrollable) {
            state.overlayHorizontal->hide();
        }
    }

    if (verticalScrollable) {
        const int bottomCut = horizontalScrollable ? ScrollBarHitThickness : 0;
        const int extent = std::max(edgeButtonExtent(state.scrollToStartButton),
                                    edgeButtonExtent(state.scrollToEndButton));
        // Use the same outer inset horizontally and vertically. The edge
        // buttons may overlap the scrollbar's otherwise generous hit padding:
        // the corresponding button is hidden when the thumb is at that edge,
        // so this keeps the chrome visually balanced without covering an
        // active end-position thumb.
        const int outerInset = ScrollBarEndInset + EdgeButtonGap;
        const int x = viewportRect.right() - outerInset - extent + 1;
        if (state.scrollToStartButton) {
            state.scrollToStartButton->setGeometry(
                x,
                viewportRect.top() + outerInset,
                extent,
                extent);
            state.scrollToStartButton->raise();
        }
        if (state.scrollToEndButton) {
            state.scrollToEndButton->setGeometry(
                x,
                viewportRect.bottom() - bottomCut - outerInset - extent + 1,
                extent,
                extent);
            state.scrollToEndButton->raise();
        }
    }
}

void OverlayScrollBarManager::updatePalette(State& state)
{
    if (!state.area || !state.area->viewport()) {
        return;
    }

    QWidget* viewport = state.area->viewport();
    const QPalette viewportPalette = viewport->palette();
    QColor background = viewportPalette.color(viewport->backgroundRole());
    if (!background.isValid()) {
        background = viewportPalette.color(QPalette::Base);
    }

    // Keep the handle neutral and let opacity, rather than hue or geometry,
    // communicate hover/idle state. This avoids accent-colored hover under
    // Breeze and keeps the shape identical throughout the animation.
    const bool darkBackground = background.lightnessF() < 0.5;
    QColor handle = darkBackground ? QColor(Qt::white) : QColor(Qt::black);
    handle.setAlpha(darkBackground ? 185 : 160);

    const QString styleSheet = overlayStyleSheet(handle);
    if (state.overlayVertical) {
        state.overlayVertical->setStyleSheet(styleSheet);
    }
    if (state.overlayHorizontal) {
        state.overlayHorizontal->setStyleSheet(styleSheet);
    }
}

void OverlayScrollBarManager::reveal(State& state)
{
    sync(state);
    layout(state);

    if (state.overlayVertical && state.verticalEnabled && scrollable(state.sourceVertical)) {
        state.overlayVertical->show();
        state.overlayVertical->raise();
        animateOpacity(state.verticalOpacity,
                       state.verticalAnimation,
                       1.0,
                       RevealDurationMs);
    }
    if (state.overlayHorizontal && state.horizontalEnabled && scrollable(state.sourceHorizontal)) {
        state.overlayHorizontal->show();
        state.overlayHorizontal->raise();
        animateOpacity(state.horizontalOpacity,
                       state.horizontalAnimation,
                       1.0,
                       RevealDurationMs);
    }
}

void OverlayScrollBarManager::fade(State& state)
{
    const bool dragging = (state.overlayVertical && state.overlayVertical->isSliderDown())
        || (state.overlayHorizontal && state.overlayHorizontal->isSliderDown());
    if (dragging || cursorOverOverlay(state)) {
        reveal(state);
        return;
    }

    animateOpacity(state.verticalOpacity,
                   state.verticalAnimation,
                   0.0,
                   FadeDurationMs);
    animateOpacity(state.horizontalOpacity,
                   state.horizontalAnimation,
                   0.0,
                   FadeDurationMs);
}

void OverlayScrollBarManager::scheduleFade(State& state)
{
    if (state.fadeTimer) {
        state.fadeTimer->start();
    }
}

bool OverlayScrollBarManager::containsCursor(const State& state) const
{
    if (!state.area || !state.area->isVisible()) {
        return false;
    }
    const QPoint local = state.area->mapFromGlobal(QCursor::pos());
    return state.area->rect().contains(local);
}

bool OverlayScrollBarManager::cursorOverOverlay(const State& state) const
{
    if (!state.area || !state.area->isVisible()) {
        return false;
    }

    const QPoint local = state.area->mapFromGlobal(QCursor::pos());
    const auto containsBar = [local](const QScrollBar* bar) {
        return bar && bar->isVisible() && scrollable(bar) && bar->geometry().contains(local);
    };
    const auto containsButton = [local](const QToolButton* button) {
        return button && button->isVisible() && button->geometry().contains(local);
    };
    return containsBar(state.overlayVertical)
        || containsBar(state.overlayHorizontal)
        || containsButton(state.scrollToStartButton)
        || containsButton(state.scrollToEndButton);
}

bool OverlayScrollBarManager::eventFilter(QObject* watched, QEvent* event)
{
    if (!watched || !event) {
        return QObject::eventFilter(watched, event);
    }

    if (auto* area = qobject_cast<QAbstractScrollArea*>(watched)) {
        if ((event->type() == QEvent::Polish || event->type() == QEvent::Show)
            && !states.contains(area)) {
            registerArea(area);
        }

        const auto it = states.constFind(area);
        if (it != states.cend()) {
            State& state = *it.value();
            switch (event->type()) {
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            case QEvent::Show:
                QTimer::singleShot(0, area, [this, statePtr = it.value()] {
                    sync(*statePtr);
                    layout(*statePtr);
                    statePtr->cursorInside = containsCursor(*statePtr);
                    if (statePtr->cursorInside) {
                        reveal(*statePtr);
                        scheduleFade(*statePtr);
                    }
                });
                break;
            case QEvent::PaletteChange:
            case QEvent::ApplicationPaletteChange:
            case QEvent::StyleChange:
                updatePalette(state);
                layout(state);
                break;
            case QEvent::FontChange:
                if (state.scrollToStartButton) {
                    state.scrollToStartButton->setFont(edgeButtonFont(*state.area));
                }
                if (state.scrollToEndButton) {
                    state.scrollToEndButton->setFont(edgeButtonFont(*state.area));
                }
                layout(state);
                break;
            case QEvent::DynamicPropertyChange:
                sync(state);
                layout(state);
                break;
            case QEvent::Hide:
                state.cursorInside = false;
                if (state.fadeTimer) {
                    state.fadeTimer->stop();
                }
                if (state.verticalAnimation) {
                    state.verticalAnimation->stop();
                }
                if (state.horizontalAnimation) {
                    state.horizontalAnimation->stop();
                }
                if (state.verticalOpacity) {
                    state.verticalOpacity->setOpacity(0.0);
                }
                if (state.horizontalOpacity) {
                    state.horizontalOpacity->setOpacity(0.0);
                }
                break;
            default:
                break;
            }
        }
    }

    QWidget* widget = qobject_cast<QWidget*>(watched);
    State* state = widget ? stateForWidget(widget) : nullptr;
    if (state) {
        const bool overlayWidget = widget == state->overlayVertical
            || widget == state->overlayHorizontal
            || widget == state->scrollToStartButton
            || widget == state->scrollToEndButton;
        const bool areaBoundary = widget == state->area
            || (state->area && widget == state->area->viewport());

        switch (event->type()) {
        case QEvent::Enter:
            if (widget == state->scrollToStartButton
                || widget == state->scrollToEndButton) {
                if (auto* opacity =
                        qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect())) {
                    opacity->setOpacity(EdgeButtonHoverOpacity);
                }
            }
            if (overlayWidget) {
                scheduleFade(*state);
                reveal(*state);
            } else if (areaBoundary && !state->cursorInside && containsCursor(*state)) {
                state->cursorInside = true;
                reveal(*state);
                scheduleFade(*state);
            }
            break;
        case QEvent::Leave:
            if (widget == state->scrollToStartButton
                || widget == state->scrollToEndButton) {
                if (auto* opacity =
                        qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect())) {
                    opacity->setOpacity(EdgeButtonIdleOpacity);
                }
            }
            if (overlayWidget && state->area) {
                QTimer::singleShot(0, state->area, [this, state] {
                    if (cursorOverOverlay(*state)) {
                        scheduleFade(*state);
                        reveal(*state);
                    } else if (containsCursor(*state)) {
                        state->cursorInside = true;
                        scheduleFade(*state);
                    } else {
                        state->cursorInside = false;
                        fade(*state);
                    }
                });
            } else if (areaBoundary && state->area) {
                QTimer::singleShot(0, state->area, [this, state] {
                    if (!containsCursor(*state)) {
                        state->cursorInside = false;
                        fade(*state);
                    }
                });
            }
            break;
        case QEvent::Wheel:
            state->cursorInside = containsCursor(*state);
            reveal(*state);
            scheduleFade(*state);
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

} // namespace Mattermost
