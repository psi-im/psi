/*
 * Copyright (C) 2007  Remko Troncon
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

#include "iodeviceopener.h"


IODeviceOpener::IODeviceOpener(QIODevice* device, QIODevice::OpenModeFlag mode) : device_(device), close_(false)
{
	if (!device_->isOpen()) {
		if (!device_->open(mode)) {
			isOpen_ = false;
			return;
		}
		close_ = true;
	}
	else if (!(device_->openMode() & mode)) {
		isOpen_ = false;
		return;
	}
	isOpen_ = true;
}

IODeviceOpener::~IODeviceOpener()
{
	if (close_) {
		device_->close();
	}
}
