/*
 * filetunecontroller.h
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef FILEPLAYERCONTROLLER_H
#define FILEPLAYERCONTROLLER_H

namespace QCA {
    class FileWatch;
}

#include "tune.h"
#include "pollingtunecontroller.h"

class FileTuneController : public PollingTuneController
{
    Q_OBJECT

    enum Mode {
        ModeWatch,
        ModeBoth
    };

public:
    FileTuneController(const QString& file);
    Tune currentTune() const;

protected slots:
    void check();

private:
    QString _songFile;
    QCA::FileWatch *_tuneFileWatcher;
    Tune _currentTune;
    bool _waitForCreated;
    bool _watchFunctional; // is able to work at all (for example it is known NFS doesn't support fs notifications)
};

#endif
