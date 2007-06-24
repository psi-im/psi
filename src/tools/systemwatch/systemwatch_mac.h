/*
 * systemwatch_mac.h - Detect changes in the system state (Mac OS X).
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

#ifndef SYSTEMWATCH_MAC_H
#define SYSTEMWATCH_MAC_H

#include "systemwatch.h"

class MacSystemWatch : public SystemWatch
{
public:
	MacSystemWatch();

	// These shouldn't be called from outside, but i can't hide them atm.
	void emitSleep() { emit sleep(); }
	void emitIdleSleep() { emit idleSleep(); }
	void emitWakeup() { emit wakeup(); }
};

#endif
