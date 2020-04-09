/*
 * cocoacommon.mm - common utilities for cocoa framework
 * Copyright (C) 2010  Tobias Markmann
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

#include "cocoacommon.h"

#include <Cocoa/Cocoa.h>

int macosCommonFirstWeekday()
{
    int         firstDay;
    NSCalendar *cal = [NSCalendar currentCalendar];
    firstDay        = ([cal firstWeekday] + 5) % 7 + 1;
    return firstDay;
}
