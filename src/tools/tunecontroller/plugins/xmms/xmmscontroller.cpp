/*
 * xmmscontroller.cpp 
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

#include <xmms/xmmsctrl.h>

#include "xmmscontroller.h"


/**
 * \class XMMSController
 * \brief A controller for XMMS.
 */


/**
 * \brief Constructs the controller.
 */
XMMSController::XMMSController() : PollingTuneController()
{
}


Tune XMMSController::currentTune()
{
	Tune tune;
	if (xmms_remote_is_running(0) && xmms_remote_is_playing(0)) {
		int pos =  xmms_remote_get_playlist_pos(0);
		char* c_title = xmms_remote_get_playlist_title(0,pos);
		if (c_title) {
			tune.setName(QString(c_title));
			free(c_title);
		}
		tune.setTime(xmms_remote_get_playlist_time(0,pos) / 1000);
	}
	return tune;
}
