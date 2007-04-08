/*
 * rosteritemexchangetask.h
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

#ifndef ROSTERITEMEXCHANGETASK_H
#define ROSTERITEMEXCHANGETASK_H

#include <QDomElement>

#include "xmpp_task.h"
#include "xmpp_rosterx.h"

using namespace XMPP;

class RosterItemExchangeTask : public Task
{
	Q_OBJECT

public:
	RosterItemExchangeTask(Task*);

	bool take(const QDomElement&);
	void setIgnoreNonRoster(bool);

signals:
	void rosterItemExchange(const Jid&, const RosterExchangeItems&);

private:
	bool ignoreNonRoster_;
};

#endif
