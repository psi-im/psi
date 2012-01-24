/*
 * psiactions.h - stanza activation types
 * Copyright (C) 2007  Michail Pishchagin
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

#ifndef PSIACTIONS_H
#define PSIACTIONS_H

enum ActivationType {
	UserAction = 0,    // action is triggered by user
	UserPassiveAction, // action is triggered by user, but should be performed in background
	IncomingStanza,    // action is triggered by incoming stanza
	FromXml            // action is triggered by restoring saved events from XML
};

#endif
