/*
 * adhoc_fileserver.h - Implementation of a personal file server using ad-hoc 
 *		commands
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

#ifndef AHFILESERVER_H
#define AHFILESERVER_H

#include "adhoc.h"

class AHFileServer : public AHCommandServer
{
public:
	AHFileServer(AHCServerManager* m) : AHCommandServer(m) { }
	virtual QString node() const 
		{ return QString("http://psi.affinix.com/commands/files"); }
	virtual bool isAllowed(const Jid&) const;
	virtual QString name() const { return QString("Send file"); }
	virtual AHCommand execute(const AHCommand& c, const Jid&);
};

#endif
