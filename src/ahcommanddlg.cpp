/*
 * ahcommanddlg.cpp - Ad-Hoc Command Dialog
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

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLayout>
#include <QLabel>

#include "ahcexecutetask.h" 
#include "ahcommanddlg.h"
#include "busywidget.h"
#include "psiaccount.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_client.h"

using namespace XMPP;

#define AHC_NS "http://jabber.org/protocol/commands"

// -------------------------------------------------------------------------- 

static bool operator<(const AHCommandItem &ci1, const AHCommandItem &ci2)
{
	return ci1.name < ci2.name;
}

// -------------------------------------------------------------------------- 
// JT_AHCGetList: A Task to retreive the available commands of a client
// -------------------------------------------------------------------------- 

class JT_AHCGetList : public Task
{
public:
	JT_AHCGetList(Task* t, const Jid& j);

	void onGo();
	bool take(const QDomElement &x);
	
	const QList<AHCommandItem>& commands() const { return commands_; }

private:
	Jid receiver_;
	QList<AHCommandItem> commands_;
};


JT_AHCGetList::JT_AHCGetList(Task* t, const Jid& j) : Task(t), receiver_(j)
{
}

void JT_AHCGetList::onGo()
{
	QDomElement e = createIQ(doc(), "get", receiver_.full(), id());
	QDomElement q = doc()->createElement("query");
	q.setAttribute("xmlns", "http://jabber.org/protocol/disco#items");
	q.setAttribute("node", AHC_NS);
	e.appendChild(q);
	send(e);
}

bool JT_AHCGetList::take(const QDomElement& e)
{
	if(!iqVerify(e, receiver_, id())) {
		return false;
	}
	
	if (e.attribute("type") == "result") {
		// Extract commands
		commands_.clear();
		bool found;
		QDomElement commands = findSubTag(e, "query", &found);
		if(found) {
			for(QDomNode n = commands.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if(i.isNull())
					continue;

				if(i.tagName() == "item") {
					AHCommandItem ci;
					ci.node = i.attribute("node");
					ci.name = i.attribute("name");
					ci.jid = i.attribute("jid");
					commands_ += ci;
				}
			}
			qSort(commands_);
		}
		setSuccess();
		return true;
	}
	else {
		setError(e);
		return false;
	}
}


// -------------------------------------------------------------------------- 
// JT_AHCommandDlg: Initial command dialog
// -------------------------------------------------------------------------- 

AHCommandDlg::AHCommandDlg(PsiAccount* pa, const Jid& receiver)
	: QDialog(0), pa_(pa), receiver_(receiver)
{
	setAttribute(Qt::WA_DeleteOnClose);
	QVBoxLayout *vb = new QVBoxLayout(this, 11, 6);

	// Command list + Buttons
	QLabel* lb_commands = new QLabel(tr("Command:"),this);
	vb->addWidget(lb_commands);
	cb_commands = new QComboBox(this);
	vb->addWidget(cb_commands);
	/*pb_info = new QPushButton(tr("Info"), this);
	hb1->addWidget(pb_info);*/

	// Refresh button
	//pb_refresh = new QPushButton(tr("Refresh"), this);
	//hb2->addWidget(pb_refresh);
	//connect(pb_refresh, SIGNAL(clicked()), SLOT(refreshCommands()));

	vb->addStretch(1);

	// Bottom row
	QHBoxLayout *hb2 = new QHBoxLayout(vb);
	busy_ = new BusyWidget(this);
	hb2->addWidget(busy_);
	hb2->addItem(new QSpacerItem(20,0,QSizePolicy::Expanding));
	pb_execute = new QPushButton(tr("Execute"), this);
	hb2->addWidget(pb_execute);
	connect(pb_execute, SIGNAL(clicked()), SLOT(executeCommand()));
	pb_close = new QPushButton(tr("Close"), this);
	hb2->addWidget(pb_close);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	pb_close->setDefault(true);
	pb_close->setFocus();

	setCaption(QString("Execute Command (%1)").arg(receiver.full()));

	// Load commands
	refreshCommands();
}


void AHCommandDlg::refreshCommands()
{
	cb_commands->clear();
	pb_execute->setEnabled(false);
	//pb_info->setEnabled(false);

	busy_->start();
	JT_AHCGetList* t= new JT_AHCGetList(pa_->client()->rootTask(),receiver_);
	connect(t,SIGNAL(finished()),SLOT(listReceived()));
	t->go(true);
}

void AHCommandDlg::listReceived()
{
	JT_AHCGetList* task_list = (JT_AHCGetList*) sender();
	foreach(AHCommandItem i, task_list->commands()) {
		cb_commands->insertItem(i.name);	
		commands_.append(i);
	}
	pb_execute->setEnabled(cb_commands->count()>0);
	busy_->stop();
}

void AHCommandDlg::executeCommand()
{
	if (cb_commands->count() > 0) {
		busy_->start();
		Jid to(commands_[cb_commands->currentItem()].jid);
		QString node = commands_[cb_commands->currentItem()].node;
		AHCExecuteTask* t = new AHCExecuteTask(to,AHCommand(node),pa_->client()->rootTask());
		connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
		t->go(true);
	}
}

void AHCommandDlg::commandExecuted()
{
	busy_->stop();
	close();
}

void AHCommandDlg::executeCommand(XMPP::Client* c, const XMPP::Jid& to, const QString &node)
{
	AHCExecuteTask* t = new AHCExecuteTask(to,AHCommand(node),c->rootTask());
	t->go(true);
}
