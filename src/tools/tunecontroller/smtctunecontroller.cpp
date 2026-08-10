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

#include <winrt/base.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.media.control.h>

/**
 * \class SmtcTuneController
 * \brief A controller class for a Windows SMTC protocol.
 */

using namespace winrt;
using namespace Windows::Media::Control;
using namespace winrt::Windows::Foundation;

namespace {
thread_local bool g_tryInit = false;
}

void initWinRt()
{
    // Hack to init winrt without app crash
    if (g_tryInit)
        return;
    g_tryInit = true;
    try {
        winrt::init_apartment();
    } catch (const winrt::hresult_error &ex) {
        // RPC_E_CHANGED_MODE (0x80010106) — normal for plugin,
        // means, that COM already initialized in STA mode.
        if (ex.code() == hresult(0x80010106)) {
            // COM is ready, ignoring
        }
    }
}

SmtcTuneController::SmtcTuneController() : PollingTuneController(), _tuneSent(false)
{
    startAsync(); // start async smtc listening
    startPoll();  // start polling
}

void SmtcTuneController::check() { PollingTuneController::check(); }

Tune SmtcTuneController::currentTune() const { return _currentTune; }

winrt::Windows::Foundation::IAsyncAction SmtcTuneController::startAsync()
{
    // connect to winrt and subscribe to current session
    initWinRt();
    auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    if (manager) {
        manager.CurrentSessionChanged([this, manager](GlobalSystemMediaTransportControlsSessionManager const &sender,
                                                      IInspectable const & /*args*/) {
            auto currentSession = sender.GetCurrentSession();
            if (currentSession)
                subscribeToSession(currentSession);
        });
        // obtain media data if player is playing
        auto session = manager.GetCurrentSession();
        if (session)
            getMediaProperties(session);
    }
    co_return;
}

winrt::Windows::Foundation::IAsyncAction
SmtcTuneController::getMediaProperties(GlobalSystemMediaTransportControlsSession session)
{
    initWinRt();
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
    }
    co_return;
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

void SmtcTuneController::subscribeToSession(const GlobalSystemMediaTransportControlsSession &session)
{
    if (session) {
        // metadata changed callback
        session.MediaPropertiesChanged([this](GlobalSystemMediaTransportControlsSession const &sender, auto const &) {
            auto playbackInfo = sender.GetPlaybackInfo();
            auto mediaType    = playbackInfo.PlaybackType().Value();
            if (mediaType != Windows::Media::MediaPlaybackType::Video)
                getMediaProperties(sender);
        });
        // playback status changed callback
        session.PlaybackInfoChanged([this](GlobalSystemMediaTransportControlsSession const &sender, auto const &) {
            auto playbackInfo = sender.GetPlaybackInfo();
            auto mediaType    = playbackInfo.PlaybackType().Value();
            if (mediaType != Windows::Media::MediaPlaybackType::Video) {
                auto status = playbackInfo.PlaybackStatus();
                if (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
                    getMediaProperties(sender);
                else
                    emit clearTune();
            }
        });
    }
}
