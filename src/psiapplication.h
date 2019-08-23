/*
 * psiapplication.h - subclass of QApplication to do some workarounds
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

#ifndef PSIAPPLICATION_H
#define PSIAPPLICATION_H

#include <QApplication>
#ifdef Q_OS_MAC
#    include <Carbon/Carbon.h>
#endif

class QEvent;

class PsiApplication : public QApplication
{
    Q_OBJECT
public:
    PsiApplication(int &argc, char **argv, bool GUIenabled = true);
    ~PsiApplication();

#ifdef Q_OS_MAC
    bool macEventFilter(EventHandlerCallRef, EventRef);
#endif

signals:
    void dockActivated();

private:
    void init(bool GUIenabled);
};

#endif // PSIAPPLICATION_H
