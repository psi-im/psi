/*
 * filetunecontroller.cpp 
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QFile>
#include <QTimer>
#include <QTextStream>

#include "tune.h"
#include "filetunecontroller.h"

#define CHECK_INTERVAL 5000

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
FileTuneController::FileTuneController(const QString& songFile) : PollingTuneController(), songFile_(songFile)
{
}


Tune FileTuneController::currentTune()
{
	Tune tune;
	QFile file(songFile_);
	if (file.open(QIODevice::ReadOnly)) {
		QTextStream stream( &file );
		stream.setCodec("UTF-8");
		stream.setAutoDetectUnicode(true);
		tune.setName(stream.readLine());
		tune.setArtist(stream.readLine());
		tune.setAlbum(stream.readLine());
		tune.setTrack(stream.readLine());
		tune.setTime(stream.readLine().toUInt());
	}
	return tune;
}
