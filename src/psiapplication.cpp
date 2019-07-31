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

#ifdef Q_OS_MAC
#    include <Carbon/Carbon.h>
#endif
#include <QSessionManager>

#ifdef Q_OS_MAC
#    include "CocoaUtilities/CocoaTrayClick.h"
#endif
#include "resourcemenu.h"
#ifdef Q_OS_WIN
#    include "systemwatch/systemwatch_win.h"
#endif

//----------------------------------------------------------------------------
// PsiApplication
//----------------------------------------------------------------------------

PsiApplication::PsiApplication(int &argc, char **argv, bool GUIenabled)
:    QApplication(argc, argv, GUIenabled)
{
    init(GUIenabled);
#ifdef Q_OS_MAC
    connect(CocoaTrayClick::instance(), SIGNAL(trayClicked()), this, SIGNAL(dockActivated()));
#endif
}

PsiApplication::~PsiApplication()
{

}

void PsiApplication::init(bool GUIenabled)
{
    Q_UNUSED(GUIenabled);
#ifdef Q_OS_MAC
    setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif
}

#ifdef Q_OS_MAC
bool PsiApplication::macEventFilter( EventHandlerCallRef, EventRef inEvent )
{
    UInt32 eclass = GetEventClass(inEvent);
    int etype = GetEventKind(inEvent);
    if(eclass == 'eppc' && etype == kEventAppleEvent) {
        dockActivated();
    }
    return false;
}
#endif
