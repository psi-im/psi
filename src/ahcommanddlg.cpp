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

#include "ahcommanddlg.h"

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLayout>
#include <QLabel>

#include "ahcexecutetask.h" 
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
	ui_.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);

	pb_close = ui_.buttonBox->button(QDialogButtonBox::Cancel);
	pb_execute = ui_.buttonBox->addButton(tr("Execute"), QDialogButtonBox::AcceptRole);
	connect(pb_execute, SIGNAL(clicked()), SLOT(executeCommand()));
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	pb_execute->setDefault(true);

	setWindowTitle(QString("Execute Command (%1)").arg(receiver.full()));

	refreshCommands();
	adjustSize();
}

void AHCommandDlg::refreshCommands()
{
	ui_.cb_commands->clear();
	pb_execute->setEnabled(false);

	ui_.busy->start();
	JT_AHCGetList* t= new JT_AHCGetList(pa_->client()->rootTask(),receiver_);
	connect(t,SIGNAL(finished()),SLOT(listReceived()));
	t->go(true);
}

void AHCommandDlg::listReceived()
{
	JT_AHCGetList* task_list = (JT_AHCGetList*) sender();
	foreach(AHCommandItem i, task_list->commands()) {
		ui_.cb_commands->addItem(i.name);
		commands_.append(i);
	}
	pb_execute->setEnabled(ui_.cb_commands->count()>0);
	ui_.busy->stop();
}

void AHCommandDlg::executeCommand()
{
	if (ui_.cb_commands->count() > 0) {
		ui_.busy->start();
		Jid to(commands_[ui_.cb_commands->currentIndex()].jid);
		QString node = commands_[ui_.cb_commands->currentIndex()].node;
		AHCExecuteTask* t = new AHCExecuteTask(to,AHCommand(node),pa_->client()->rootTask());
		connect(t,SIGNAL(finished()),SLOT(commandExecuted()));
		t->go(true);
	}
}

void AHCommandDlg::commandExecuted()
{
	ui_.busy->stop();
	close();
}

void AHCommandDlg::executeCommand(XMPP::Client* c, const XMPP::Jid& to, const QString &node)
{
	AHCExecuteTask* t = new AHCExecuteTask(to,AHCommand(node),c->rootTask());
	t->go(true);
}
