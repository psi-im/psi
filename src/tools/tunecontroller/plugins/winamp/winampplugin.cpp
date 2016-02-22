/*
 * winampplugin.cpp
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

#include "winampplugin.h"


QString WinAmpPlugin::name()
{
	return "WinAmp";
}

TuneController* WinAmpPlugin::createController()
{
	return new WinAmpTuneController();
}

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN2(winampplugin, WinAmpPlugin);
#endif
