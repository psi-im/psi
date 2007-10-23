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

#ifndef IODEVICEOPENER_H
#define IODEVICEOPENER_H

#include <QIODevice>
#include <QPointer>

/**
 * An IODeviceOpener is used to ensure that an IODevice is opened.
 * If the QIODevice was not open when the opener was created, it will be closed
 * again when the opener is destroyed.
 *
 * Example:
 *		void foo(QIODevice* device) {
 *			IODeviceOpener opener(device, QIODevice::ReadOnly); 
 *			if (!opener.isOpen()) {
 *				qDebug() << "Error opening QIODevice";
 *				return;
 *			}
 *			...
 *			device->readAll()
 *			...
 *		}
 */
class IODeviceOpener
{
public:
	/**
	 * Opens an QIODevice in a specific mode if the device was not opened
	 * yet.
	 * If the device was already open but in a different, non-compatible
	 * mode than the one requested, isOpen() will return false.
	 */
	IODeviceOpener(QIODevice* device, QIODevice::OpenModeFlag mode);

	/**
	 * Closes the QIODevice passed to the constructor if the IODevice was not
	 * opened at that time.
	 */
	~IODeviceOpener();

	/**
	 * Checks whether the io device was opened succesfully in the mode
	 * requested.
	 */
	bool isOpen() const { return isOpen_; }

private:
	QPointer<QIODevice> device_;
	bool close_;
	bool isOpen_;
};

#endif
