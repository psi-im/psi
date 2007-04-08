/*
 * pollingtunecontroller.cpp 
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

#include <QTimer>

#include "tune.h"
#include "pollingtunecontroller.h"

/**
 * \class PollingTuneController
 * \brief A base class for a controller that uses polling to retrieve the  currently playing song.
 *
 * An implementing class only has to implement currentTune(), and the correct
 * signals will be emitted.
 */


/**
 * \brief Constructs the controller.
 */
PollingTuneController::PollingTuneController()
{
	connect(&timer_, SIGNAL(timeout()), SLOT(check()));
	timer_.start(3000);
}


/**
 * \brief Sets the polling interval. If interval is 0, no polling is
 * done.
 */
void PollingTuneController::setInterval(int interval)
{
	if (interval > 0)
		timer_.start(interval);
	else
		timer_.stop();
}

/**
 * Polls for new song info.
 */
void PollingTuneController::check()
{
	Tune tune = currentTune();
	if (prev_tune_ != tune) {
		prev_tune_ = tune;
		if (tune.isNull()) {
			emit stopped();
		}
		else {
			emit playing(tune);
		}
	}
}
