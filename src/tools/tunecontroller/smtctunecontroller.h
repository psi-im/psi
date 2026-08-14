/*
 * smtctunecontroller.h
 * Copyright (C) 2026  Vitaly Tonkacheyev
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

#ifndef SMTCTUNECONTROLLER_H
#define SMTCTUNECONTROLLER_H

#include "pollingtunecontroller.h"
#include "tune.h"
#include <winrt/windows.foundation.h>
#include <winrt/windows.media.control.h>

class SmtcTuneController : public PollingTuneController {
    Q_OBJECT

public:
    SmtcTuneController();
    ~SmtcTuneController();
    Tune currentTune() const;

protected slots:
    void check();

private:
    using SessionManager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using Session        = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using PlaybackStatus = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
    using AsyncAction    = winrt::Windows::Foundation::IAsyncAction;
    AsyncAction startAsync();
    AsyncAction updateTune(Session session);
    void        initWinRt();
    void        sendTune(const Tune &tune);
    void        clearTune();
    void        updateCurrentSession(const Session &session);

private:
    Tune           _currentTune;
    bool           _tuneSent { false };
    bool           _updateInFlight { false };
    AsyncAction    _startAction { nullptr };
    AsyncAction    _updateAction { nullptr };
    SessionManager _manager { nullptr };
    Session        _currentSession { nullptr };
};

#endif // SMTCTUNECONTROLLER_H
