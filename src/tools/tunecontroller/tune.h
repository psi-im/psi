/*
 * tune.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef TUNE_H
#define TUNE_H

#include <QString>

/**
 * \brief A class represinting a tune.
 */
class Tune
{
public:
	/**
	 * \brief Constructs an empty tune
	 */
	Tune() : time_(0) { }

	const QString& name() const { return name_; }
	const QString& artist() const { return artist_; }
	const QString& album() const { return album_; }
	const QString& track() const { return track_; }
	const QString& url() const { return url_; }

	unsigned int time() const { return time_; }

	/**
	 * \brief Returns a string representation of the tune's playing time.
	 */
	QString timeString() const {
		return QString("%1:%2").arg(time_ / 60).arg(time_ % 60, 2, 10, QChar('0'));
	}

	/**
	 * \brief Checks whether this is a null tune.
	 */
	bool isNull() const {
		return name_.isEmpty() && artist_.isEmpty() && album_.isEmpty() && track_.isEmpty() && url_.isEmpty() && time_ == 0;
	}

	/**
	 * \brief Compares this tune with another tune for equality.
	 */
	bool operator==(const Tune& t) const {
		return name_ == t.name_ && artist_ == t.artist_ && album_ == t.album_ && track_ == t.track_ && url_ == t.url_ && time_ == t.time_;
	}

	/**
	 * \brief Compares this tune with another tune for inequality.
	 */
	bool operator!=(const Tune& t) const {
		return !((*this) == t);
	}

	void setName(const QString& name) { name_ = name; }
	void setArtist(const QString& artist) { artist_ = artist; }
	void setAlbum(const QString& album) { album_ = album; }
	void setTrack(const QString& track) { track_ = track; }
	void setURL(const QString& url) { url_ = url; }
	void setTime(unsigned int time) { time_ = time; }

private:
	QString name_, artist_, album_, track_, url_;
	unsigned int time_;
};

#endif
