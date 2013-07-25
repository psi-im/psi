/*
 * filetunecontroller.cpp
 * Copyright (C) 2006  Remko Troncon
 * 2011 Vitaly Tonkacheyev, rion
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

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include "qca.h"
#include "filetunecontroller.h"

/**
 * \class FileTuneController
 * \brief A player-independent class for controlling any player through files.
 * This class reads the current playing song from a file, and notifies if
 * the song changed. Therefore, this class can be used if specific support for
 * a certain player isn't implemented yet.
 *
 * A song file has to contain one line per record, with the records in the
 * following order: track name, track artist, track album, track number, track
 * time.
 */


/**
 * \brief Constructs the controller.
 * \param songFile the filename from which the currently playing song is
 *		read.
 */
FileTuneController::FileTuneController(const QString& songFile)
	: PollingTuneController()
	, _songFile(songFile)
	, _waitForCreated(true)
	, _watchFunctional(false)
{
	// old mechanism of work with tune file
	startPoll();
	// new mechanism of work with tune file
	// let's consider the directory _songFile resides exists. other cases should be solved by reconfiguration is restarting.
	// in cases when directory does not exist polling will work as expected. we will not try turn on the watching.
	// on the other hand if watch is recognized as functional polling will be disabled, so the user
	// should understand if he suddenly removed the directory with watched file tunes won't work at all.
	_tuneFileWatcher = new QCA::FileWatch(_songFile, this); // qca watch works on non-existing files ;)
	connect(_tuneFileWatcher, SIGNAL(changed()), this, SLOT(onFileChanged()));
}

Tune FileTuneController::currentTune() const
{
	return _currentTune;
}

void FileTuneController::onFileChanged() // this will never happen if _songFile's directory doesn't exist
{
	_watchFunctional = true;
	check();
}

void FileTuneController::check()
{
	Tune existedTune = _currentTune;
	_currentTune = Tune(); // just a reset
	if (QFile::exists(_songFile)) {
		if (_waitForCreated && _watchFunctional) {
			if (isPolling()) {
				stopPoll();
			}
			_waitForCreated = false;
		}
		QFile file(_songFile);
		if (file.open(QIODevice::ReadOnly)) {
			QTextStream stream( &file );
			stream.setCodec("UTF-8");
			stream.setAutoDetectUnicode(true);
			_currentTune.setName(stream.readLine());
			_currentTune.setArtist(stream.readLine());
			_currentTune.setAlbum(stream.readLine());
			_currentTune.setTrack(stream.readLine());
			_currentTune.setTime(stream.readLine().toUInt());
		}
	}
	else if (!_waitForCreated && existedTune.isNull()) {
		_waitForCreated = true;
		return; // we will return to this function when file created. just exit for now.
	}
	PollingTuneController::check();
}
