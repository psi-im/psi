/*
 * debug.cpp - debug staff
 * Copyright (C) 2017  Ivan Romanov <drizt@land.ru>
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

#include "debug.h"

#include <config.h>

#include <QDir>

SlowTimer::SlowTimer(const QString &path, int line, int maxTime, const QString &message)
	: _path(QDir::fromNativeSeparators(path))
	, _line(line)
	, _message(message)
	, _maxTime(maxTime)
{
	_timer.start();
}

SlowTimer::~SlowTimer ()
{
	int t = _timer.elapsed();
	if (t >= _maxTime) {
		const int stripSz = (int)(sizeof(__FILE__) - sizeof("src/debug.cpp"));
		QString relPath = _path.size() > stripSz ? _path.mid(stripSz) : _path;
		if (_message.isEmpty())
			WARNING() << "[slow]" << QString("%1:%2 %3 milliseconds").arg(relPath).arg(_line).arg(t);
		else
			WARNING() << "[slow]" << QString("%1:%2 %3 %4 milliseconds").arg(relPath).arg(_line).arg(_message).arg(t);
	}
}
