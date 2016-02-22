/*
 * aimptunecontroller.cpp
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "aimptunecontroller.h"
#include "plugins/aimp/third-party/apiRemote.h"

/**
 * \class AimpTuneController
 * \brief A controller class for AIMP3 player.
 */

static const int PLAYING = 2;
static const int STOPPED = 0;
static const WCHAR* AIMP_REMOTE_CLASS = (WCHAR *)L"AIMP2_RemoteInfo";

AimpTuneController::AimpTuneController()
: PollingTuneController(),
  _tuneSent(false)
{
	startPoll();
}

HWND AimpTuneController::findAimp() const
{
	return FindWindow(AIMP_REMOTE_CLASS, AIMP_REMOTE_CLASS);
}

int AimpTuneController::getAimpStatus(const HWND &aimp) const
{
	if (aimp) {
		return (int)SendMessage(aimp, WM_AIMP_PROPERTY, AIMP_RA_PROPERTY_PLAYER_STATE | AIMP_RA_PROPVALUE_GET, 0);
	}
	return STOPPED;
}

void AimpTuneController::check()
{
	HWND aimp = findAimp();
	if (getAimpStatus(aimp) == PLAYING) {
		sendTune(getTune());
	}
	else {
		clearTune();
	}
	PollingTuneController::check();
}

Tune AimpTuneController::currentTune() const
{
	return _currentTune;
}

Tune AimpTuneController::getTune() const
{
	HANDLE aFile=OpenFileMapping(FILE_MAP_READ, TRUE, AIMP_REMOTE_CLASS);
	PAIMPRemoteFileInfo aInfo = (PAIMPRemoteFileInfo)MapViewOfFile(aFile, FILE_MAP_READ, 0, 0, AIMPRemoteAccessMapFileSize);
	if (aInfo != NULL) {
		wchar_t *str = (wchar_t *)((char*)aInfo + sizeof(*aInfo));
		QString album = QString::fromWCharArray(str, aInfo->AlbumLength);
		str += aInfo->AlbumLength;
		QString artist = QString::fromWCharArray(str, aInfo->ArtistLength);
		str += aInfo->ArtistLength + aInfo->DateLength;
		QString url = QString::fromWCharArray(str, aInfo->FileNameLength);
		str += aInfo->FileNameLength + aInfo->GenreLength;
		QString title = QString::fromWCharArray(str, aInfo->TitleLength);
		unsigned long trackNumber = aInfo->TrackNumber;
		unsigned long time = aInfo->Duration;
		Tune tune = Tune();
		if (!url.isEmpty()) {
			if (!title.isEmpty()) {
				tune.setName(title);
			}
			else {
				int index = url.replace("/", "\\").lastIndexOf("\\");
				if (index > 0) {
					QString filename = url.right(url.length()-index-1);
					index = filename.lastIndexOf(".");
					title = (index > 0) ? filename.left(index) : filename;
				}
				else {
					title = url;
				}
				tune.setName(title);
			}
			if (trackNumber > 0) {
				tune.setTrack(QString::number(trackNumber));
			}
			if (time > 0) {
				tune.setTime((uint)time);
			}
			if (!artist.isEmpty()) {
				tune.setArtist(artist);
			}
			if (!album.isEmpty()) {
				tune.setAlbum(album);
			}
			tune.setURL(url);
		}
		return tune;
	}
	UnmapViewOfFile(aInfo);
	CloseHandle(aFile);
	return Tune();
}

void AimpTuneController::sendTune(const Tune &tune)
{
	if (tune != _currentTune && !tune.isNull()) {
		_currentTune = tune;
		_tuneSent = true;
	}
}

void AimpTuneController::clearTune()
{
	if (_tuneSent) {
		_currentTune = Tune();
		_tuneSent = false;
	}
}
