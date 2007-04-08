/*
 * combinedtunecontroller.cpp 
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

#include "tune.h"
#include "combinedtunecontroller.h"
#include "tunecontrollermanager.h"

/**
 * \class CombinedTuneController
 * \brief Combines all supported controllers into one controller.
 */


CombinedTuneController::CombinedTuneController() 
{
	foreach(QString name, TuneControllerManager::instance()->controllerNames()) {
		TuneController* c = TuneControllerManager::instance()->createController(name);
		connect(c,SIGNAL(stopped()),SIGNAL(stopped()));
		connect(c,SIGNAL(playing(const Tune&)),SIGNAL(playing(const Tune&)));
		controllers_ += c;
	}
}

CombinedTuneController::~CombinedTuneController()
{
	while (!controllers_.isEmpty()) {
		delete controllers_.takeFirst();
	}
}

Tune CombinedTuneController::currentTune() 
{
	foreach(TuneController* c, controllers_) {
		Tune t = c->currentTune();
		if (!t.isNull())
			return t;
	}
	return Tune();
}
