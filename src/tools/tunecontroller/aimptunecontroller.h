/*
 * aimptunecontroller.h
 * Copyright (C) 2012 Vitaly Tonkacheyev
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

#ifndef AIMPTUNECONTROLLER_H
#define AIMPTUNECONTROLLER_H

#include "pollingtunecontroller.h"
#include "tune.h"
#include "windows.h"

class AimpTuneController : public PollingTuneController
{
    Q_OBJECT

public:
    AimpTuneController();
    Tune currentTune() const;

protected slots:
    void check();

private:
    Tune getTune() const;
    HWND findAimp() const;
    int getAimpStatus(const HWND &aimp) const;
    void sendTune(const Tune &tune);
    void clearTune();

private:
    Tune _currentTune;
    bool _tuneSent;
};

#endif
