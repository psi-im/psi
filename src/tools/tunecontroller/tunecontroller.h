/*
 * tunecontroller.h
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

#ifndef TUNECONTROLLER_H
#define TUNECONTROLLER_H

#include "tune.h"

#include <QObject>
#include <QString>

/**
 * \brief Base class for representing a media player.
 */
class TuneController : public QObject {
    Q_OBJECT

public:
    virtual Tune currentTune() const = 0;

signals:
    /**
     * This signal is emitted when the media player started playing a tune.
     * \param tune the playing tune
     */
    void playing(const Tune &tune);

    /**
     * This signal is emitted when the media player stopped playing tunes.
     */
    void stopped();
};

#endif // TUNECONTROLLER_H
