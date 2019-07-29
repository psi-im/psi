/*
 * winamptunecontroller.h
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2006  Remko Troncon
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

#ifndef WINAMPTUNECONTROLLER_H
#define WINAMPTUNECONTROLLER_H

#include "pollingtunecontroller.h"

#include <QPair>
#include <windows.h>

class WinAmpTuneController : public PollingTuneController
{
    Q_OBJECT
public:
    WinAmpTuneController();
    virtual Tune currentTune() const;

private:
    Tune getTune(const HWND &hWnd);
    QPair<bool, QString> getTrackTitle(const HWND &waWnd) const;

protected slots:
    void check();

private:
    Tune prevTune_;
    int antiscrollCounter_;
};

#endif
