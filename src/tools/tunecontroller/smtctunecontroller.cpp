/*
 * smtctunecontroller.cpp
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

#include "smtctunecontroller.h"

#include <QDebug>
#include <QPointer>
#include <combaseapi.h>
#include <winrt/base.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;

/**
 * \class SmtcTuneController
 * \brief A controller class for a Windows SMTC protocol.
 */

SmtcTuneController::SmtcTuneController() : PollingTuneController(), _tuneSent(false)
{
    initWinRt();
    _startAction = startAsync(); // start async smtc listening
    startPoll();                 // start polling
}

SmtcTuneController::~SmtcTuneController()
{
    stopPoll();
    if (_startAction)
        _startAction.Cancel();
    if (_updateAction)
        _updateAction.Cancel();
}

void SmtcTuneController::check()
{
    if (!_manager) {
        clearTune();
        PollingTuneController::check();
        return;
    }

    auto session = _manager.GetCurrentSession();
    updateCurrentSession(session);

    if (_currentSession && !_updateInFlight) {
        _updateInFlight = true;
        _updateAction   = nullptr;
        _updateAction   = updateTune(_currentSession);
    } else if (!_currentSession) {
        clearTune();
    }

    PollingTuneController::check();
}

Tune SmtcTuneController::currentTune() const { return _currentTune; }

IAsyncAction SmtcTuneController::startAsync()
{
    // connect to winrt and get current session
    QPointer<SmtcTuneController> guard(this);
    try {
        auto manager = co_await SessionManager::RequestAsync();
        if (guard)
            guard->_manager = manager;
        ;
    } catch (winrt::hresult_error const &e) {
        qWarning() << "SMTC initialization failed:" << QString::fromWCharArray(e.message().c_str());
    }
    co_return;
}

IAsyncAction SmtcTuneController::updateTune(Session session)
{
    if (!session)
        co_return;

    bool canPublish   = false;
    auto playbackInfo = session.GetPlaybackInfo();
    if (playbackInfo) {
        if (playbackInfo.PlaybackStatus() == PlaybackStatus::Playing)
            canPublish = (playbackInfo.PlaybackType().Value() != Windows::Media::MediaPlaybackType::Video);
    }
    if (canPublish) {
        auto props = co_await session.TryGetMediaPropertiesAsync();
        if (props) {
            Tune tune;
            auto title  = QString::fromWCharArray(props.Title().c_str());
            auto artist = QString::fromWCharArray(props.Artist().c_str());
            auto album  = QString::fromWCharArray(props.AlbumTitle().c_str());
            auto track  = props.TrackNumber();
            if (!title.isEmpty())
                tune.setName(title);
            if (!artist.isEmpty())
                tune.setArtist(artist);
            if (!album.isEmpty())
                tune.setAlbum(album);
            if (track >= 0)
                tune.setTrack(QString::number(track));
            sendTune(tune);
            _updateInFlight = false;
        }
    } else
        clearTune();
    co_return;
}

void SmtcTuneController::initWinRt()
{
    APTTYPE          aptType;
    APTTYPEQUALIFIER aptTypeQualifier;

    if (FAILED(CoGetApartmentType(&aptType, &aptTypeQualifier)))
        winrt::init_apartment();
}

void SmtcTuneController::sendTune(const Tune &tune)
{
    if (tune != _currentTune && !tune.isNull()) {
        _currentTune = tune;
        _tuneSent    = true;
    }
}

void SmtcTuneController::clearTune()
{
    if (_tuneSent) {
        _currentTune = Tune();
        _tuneSent    = false;
    }
}

void SmtcTuneController::updateCurrentSession(const Session &session)
{
    if (session) {
        if (session != _currentSession) {
            _currentSession = nullptr;
            _currentSession = session;
        }
    }
}
