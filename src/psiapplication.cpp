/*
 * psiapplication.cpp - subclass of QApplication to do some workarounds
 * Copyright (C) 2003  Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psiapplication.h"

#ifdef Q_OS_MACOS
#include "CocoaUtilities/CocoaTrayClick.h"
#endif
#include "resourcemenu.h"
#ifdef Q_OS_WIN
#include "systemwatch/systemwatch_win.h"
#endif

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#endif
#include <QEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
#include <QSessionManager>
#include <QTimer>

//----------------------------------------------------------------------------
// PsiApplication
//----------------------------------------------------------------------------

PsiApplication::PsiApplication(int &argc, char **argv, bool GUIenabled) : QApplication(argc, argv, GUIenabled)
{
    init(GUIenabled);
    paletteSettledTimer_.setSingleShot(true);
    paletteSettledTimer_.setInterval(250);
    connect(&paletteSettledTimer_, &QTimer::timeout, this, [this] { schedulePaletteNotification(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // A system's automatic dusk/dawn switch can report a new color scheme
    // before Qt has installed its corresponding application palette. This
    // signal complements (rather than replaces) ApplicationPaletteChange.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { schedulePaletteNotification(true); });
#endif
#ifdef Q_OS_MACOS
    connect(CocoaTrayClick::instance(), SIGNAL(trayClicked()), this, SIGNAL(dockActivated()));
#endif
}

PsiApplication::~PsiApplication() { }

bool PsiApplication::event(QEvent *event)
{
    const bool handled = QApplication::event(event);
    if (event->type() == QEvent::ApplicationPaletteChange) {
        schedulePaletteNotification();
    } else if (event->type() == QEvent::ThemeChange) {
        // Some platforms deliver ThemeChange without a matching
        // ApplicationPaletteChange (or deliver the palette change later).
        schedulePaletteNotification(true);
    }
    return handled;
}

void PsiApplication::schedulePaletteNotification(bool afterSystemThemeChange)
{
    if (!paletteChangeScheduled_) {
        paletteChangeScheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            paletteChangeScheduled_ = false;
            emit applicationPaletteChanged();
        });
    }
    if (afterSystemThemeChange) {
        // Qt documents that colorSchemeChanged can precede the new palette.
        // Restarting this one-shot timer coalesces rapid platform events and
        // reapplies custom UI colors after the palette/style has settled.
        paletteSettledTimer_.start();
    }
}

void PsiApplication::init(bool GUIenabled)
{
    Q_UNUSED(GUIenabled);
#ifdef Q_OS_MACOS
    setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif
}

#ifdef Q_OS_MACOS
bool PsiApplication::macEventFilter(EventHandlerCallRef, EventRef inEvent)
{
    UInt32 eclass = GetEventClass(inEvent);
    int    etype  = GetEventKind(inEvent);
    if (eclass == 'eppc' && etype == kEventAppleEvent) {
        dockActivated();
    }
    return false;
}
#endif
