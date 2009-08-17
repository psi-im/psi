/*
 * lastactivitytask.cpp
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "xmpp_task.h"
#include "xmpp_jid.h"
#include "xmpp_xmlcommon.h"
#include "lastactivitytask.h"

using namespace XMPP;

LastActivityTask::LastActivityTask(const Jid& jid, Task* parent) : Task(parent), jid_(jid)
{
	iq_ = createIQ(doc(), "get", jid_.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:last");
	iq_.appendChild(query);
}

/**
 * \brief Queried entity's JID.
 */
const Jid & LastActivityTask::jid() const
{
	return jid_;
}

void LastActivityTask::onGo()
{
	send(iq_);
}

bool LastActivityTask::take(const QDomElement &x)
{
	if(!iqVerify(x, jid_, id()))
		return false;

	if(x.attribute("type") == "result") {
		bool ok = false;
		QDomElement q = queryTag(x);
		int seconds = q.attribute("seconds").toInt(&ok);
		if (ok) {
			last_time_ = QDateTime::currentDateTime().addSecs(-seconds);
		}
		last_status_ = q.text();
		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}

const QString& LastActivityTask::status() const
{
	return last_status_;
}

const QDateTime& LastActivityTask::time() const
{
	return last_time_;
}
