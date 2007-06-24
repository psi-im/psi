/*
 * systemwatch_mac.cpp - Detect changes in the system state (Mac OS X).
 * Copyright (C) 2005  Remko Troncon
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

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

#include "systemwatch_mac.h"


// -----------------------------------------------------------------------------
// Callbacks
// -----------------------------------------------------------------------------

io_connect_t  root_port;

void sleepCallBack(void* systemWatch, io_service_t, natural_t messageType, void * messageArgument)
{
	//printf("messageType %08lx, arg %08lx\n",
	//  (long unsigned int)messageType, (long unsigned int)messageArgument);
	
	MacSystemWatch* macSystemWatch = static_cast<MacSystemWatch*>(systemWatch);
	switch (messageType) {
		case kIOMessageSystemWillSleep:
			// Sleep
			macSystemWatch->emitSleep();
			IOAllowPowerChange(root_port, (long)messageArgument);
			break;
			
		case kIOMessageCanSystemSleep:
			// Idle time sleep

			// TODO: Check if file transfers are running, and don't go to sleep
			// if there are.
			//IOCancelPowerChange(root_port, (long)messageArgument);

			macSystemWatch->emitIdleSleep();
			IOAllowPowerChange(root_port, (long)messageArgument);
			break;
			
		case kIOMessageSystemHasPoweredOn:
			// Wakeup
			macSystemWatch->emitWakeup();
			break;
	}
}


// -----------------------------------------------------------------------------
// MacSystemWatch
// -----------------------------------------------------------------------------

MacSystemWatch::MacSystemWatch()
{
	// Initialize sleep callback
	IONotificationPortRef notify;
	io_object_t           anIterator;
	root_port = IORegisterForSystemPower(this, &notify, sleepCallBack, &anIterator);
	if (!root_port) {
		printf("IORegisterForSystemPower failed\n");
	}
	else {
		CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notify), kCFRunLoopCommonModes);
	}
}
