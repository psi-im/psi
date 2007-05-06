/*
 * ahcexecutetask.h - Ad-Hoc Command Execute Task
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

#ifndef AHCEXECUTETASK_H
#define AHCEXECUTETASK_H

#include "xmpp_task.h"
#include "xmpp_jid.h"
#include "ahcommand.h"

class QDomElement;

class AHCExecuteTask : public XMPP::Task
{
public:
	AHCExecuteTask(const XMPP::Jid& j, const AHCommand&, XMPP::Task* t);

	void onGo();
	bool take(const QDomElement &x);
	
private:
	XMPP::Jid receiver_;
	AHCommand command_;
};

#endif
