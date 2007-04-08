/*
 * ahcexecutetask.cpp - Ad-Hoc Command Execute Task
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

#include "ahcexecutetask.h"
#include "ahcformdlg.h"
#include "xmpp_xmlcommon.h"
#include "psiaccount.h"

using namespace XMPP;

AHCExecuteTask::AHCExecuteTask(const Jid& j, const AHCommand& command, Task* t) : Task(t), receiver_(j), command_(command)
{
}

void AHCExecuteTask::onGo()
{
	QDomElement e = createIQ(doc(), "set", receiver_.full(), id());
	e.appendChild(command_.toXml(doc(),true));
	send(e);
}

bool AHCExecuteTask::take(const QDomElement& e)
{
	if(!iqVerify(e, receiver_, id())) {
		return false;
	}
	
	// Result of a command
	if (e.attribute("type") == "result") {
		bool found;
		QDomElement i = findSubTag(e, "command", &found);
		if (found) {
			AHCommand c(i);
			if (c.status() == AHCommand::Executing) {
				AHCFormDlg *w = new AHCFormDlg(c,receiver_,client());
				w->show();
			}
			else if (c.status() == AHCommand::Completed && i.childNodes().count() > 0) {
				AHCFormDlg *w = new AHCFormDlg(c,receiver_,client(), true);
				w->show();
			}
			setSuccess();
			return true;
		}
	}
	// Error
	/*else if (e.attribute("type") == "set") {
		AHCError err(e);
		if (err.type() != None) {
			QMessageBox::critical(0, tr("Error"), AHCommand::error2description(err.type()), QMessageBox::Ok, QMessageBox::NoButton);
		}
		return true;
	}*/
	setError(e);
	return false;
}
