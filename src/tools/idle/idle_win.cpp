/*
 * idle_win.cpp - detect desktop idle time
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "idle.h"

#include <QLibrary>
#include <windows.h>

#if defined(Q_OS_WIN32) && !defined(Q_CC_GNU) && (_WIN32_WINNT < 0x0500)
typedef struct tagLASTINPUTINFO {
    UINT  cbSize;
    DWORD dwTime;
} LASTINPUTINFO, *PLASTINPUTINFO;
#endif

class IdlePlatform::Private {
public:
    Private()
    {
        GetLastInputInfo       = 0;
        IdleUIGetLastInputTime = 0;
        lib                    = 0;
    }

    typedef BOOL(__stdcall *GetLastInputInfoFunc)(PLASTINPUTINFO);
    typedef DWORD(__stdcall *IdleUIGetLastInputTimeFunc)(void);
    GetLastInputInfoFunc       GetLastInputInfo;
    IdleUIGetLastInputTimeFunc IdleUIGetLastInputTime;
    QLibrary                  *lib;
};

IdlePlatform::IdlePlatform() { d = new Private; }

IdlePlatform::~IdlePlatform()
{
    delete d->lib;
    delete d;
}

bool IdlePlatform::init()
{
    if (d->lib)
        return true;

    // try to find the built-in Windows 2000 function
    d->lib = new QLibrary("user32");
    if (d->lib->load() && (d->GetLastInputInfo = (Private::GetLastInputInfoFunc)d->lib->resolve("GetLastInputInfo"))) {
        return true;
    } else {
        delete d->lib;
        d->lib = 0;
    }

    // fall back on idleui
    d->lib = new QLibrary("idleui");
    if (d->lib->load()
        && (d->IdleUIGetLastInputTime
            = (Private::IdleUIGetLastInputTimeFunc)d->lib->resolve("IdleUIGetLastInputTime"))) {
        return true;
    } else {
        delete d->lib;
        d->lib = 0;
    }

    return false;
}

int IdlePlatform::secondsIdle()
{
    int i;
    if (d->GetLastInputInfo) {
        LASTINPUTINFO li;
        li.cbSize = sizeof(LASTINPUTINFO);
        bool ok   = d->GetLastInputInfo(&li);
        if (!ok)
            return 0;
        i = li.dwTime;
    } else if (d->IdleUIGetLastInputTime) {
        i = d->IdleUIGetLastInputTime();
    } else
        return 0;

    return (GetTickCount() - i) / 1000;
}
