/*
 * contactmanagerdlg.cpp
 * Copyright (C) 2010 Rion
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

#include <QMessageBox>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QFile>
#include <QScrollArea>
#include <QCheckBox>

#include "contactmanagerdlg.h"

#include "psiaccount.h"
#include "userlist.h"
#include "xmpp_tasks.h"
#include "psiiconset.h"
#include "vcardfactory.h"
//#include "contactview.h"


ContactManagerDlg::ContactManagerDlg(PsiAccount *pa) :
	QDialog(0, Qt::Window),
	pa_(pa),
	um(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	ui_.setupUi(this);
	pa_->dialogRegister(this);
	setWindowTitle(tr("Contacts Manager")+" - "+pa->jid().bare());
	setWindowIcon(IconsetFactory::icon("psi/action_contacts_manager").icon());

	um = new ContactManagerModel(this, pa_);
	um->reloadUsers();
	ui_.usersView->setModel(um);
	ui_.usersView->init();

	ui_.cbAction->addItem(IconsetFactory::icon("psi/sendMessage").icon(), tr("Message"), 1);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/remove").icon(), tr("Remove"), 2);
	ui_.cbAction->addItem(IconsetFactory::icon("status/ask").icon(), tr("Auth request"), 3);
	ui_.cbAction->addItem(IconsetFactory::icon("status/ask").icon(), tr("Auth grant"), 4);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/command").icon(), tr("Change domain"), 5);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/info").icon(), tr("Resolve nicks"), 6);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/browse").icon(), tr("Move to group"), 7);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/cm_export").icon(), tr("Export"), 8);
	ui_.cbAction->addItem(IconsetFactory::icon("psi/cm_import").icon(), tr("Import"), 9);
	connect(ui_.cbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(showParamField(int)));
	showParamField(0); // 0 - default index of combobox

	ui_.cmbField->insertItems(0, um->manageableFields());
	ui_.cmbMatchType->addItem(tr("Simple"), ContactManagerModel::SimpleMatch);
	ui_.cmbMatchType->addItem(tr("RegExp"), ContactManagerModel::RegexpMatch);
	connect(ui_.btnExecute, SIGNAL(clicked()), this, SLOT(executeCurrent()));
	connect(ui_.btnSelect, SIGNAL(clicked()), this, SLOT(doSelect()));

	connect(pa_->client(), SIGNAL(rosterRequestFinished(bool,int,QString)), this, SLOT(client_rosterUpdated(bool,int,QString)));
}



ContactManagerDlg::~ContactManagerDlg()
{
	pa_->dialogUnregister(this);
}

void ContactManagerDlg::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
		ui_.retranslateUi(this);
        break;
    default:
        break;
    }
}

void ContactManagerDlg::doSelect()
{
	int type = ui_.cmbMatchType->itemData(ui_.cmbMatchType->currentIndex()).toInt();
	int coumnIndex = ui_.cmbField->currentIndex();
	um->invertByMatch(coumnIndex + 1, type,  ui_.edtMatch->text());
}

void ContactManagerDlg::executeCurrent()
{
	int action = ui_.cbAction->itemData(ui_.cbAction->currentIndex()).toInt();
	QList<UserListItem *> users = um->checkedUsers();
	if (!users.count() && action != 9) {
		return;
	}
	switch (action) {
		case 1: //message
			{
				QList<XMPP::Jid> list;
				foreach (UserListItem *u, users) {
					list.append(u->jid().full());
				}
				pa_->actionSendMessage(list);
			}
			break;
		case 2: //remove
			{
				if (QMessageBox::question(this, tr("Removal confirmation"),
										  tr("Are you sure want to delete selected contacts?"),
										  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
				um->startBatch();
				foreach (UserListItem *u, users) {
					if(u->isTransport() && !Jid(pa_->client()->host()).compare(u->jid())) {
						JT_UnRegister *ju = new JT_UnRegister(pa_->client()->rootTask());
						ju->unreg(u->jid());
						ju->go(true);
					}
					JT_Roster *r = new JT_Roster(pa_->client()->rootTask());
					r->remove(u->jid());
					r->go(true);
				}
				um->clear();
				pa_->client()->rosterRequest();
			}
			break;
		case 3: //Auth request
			foreach (UserListItem *u, users) {
				pa_->dj_authReq(u->jid());
			}
			break;
		case 4: //Auth grant
			foreach (UserListItem *u, users) {
				pa_->dj_auth(u->jid());
			}
			break;
		case 5: //change domain
			changeDomain(users);
			break;
		case 6: //resolve nicks
			foreach (UserListItem *u, users) {
				VCardFactory::instance()->getVCard(u->jid(), pa_->client()->rootTask(), pa_, SLOT(resolveContactName()));
			}
			break;
		case 7:
			changeGroup(users);
			break;
		case 8: //export
			exportRoster(users);
			break;
		case 9: //import
			importRoster();
			break;
		default:
			QMessageBox::warning(this, tr("Invalid"), tr("This action is not supported atm"));
	}
}

void ContactManagerDlg::showParamField(int index)
{
	int action = ui_.cbAction->itemData(index).toInt();
	ui_.edtActionParam->hide();
	ui_.cmbActionParam->hide();
	switch (action) {
		case 5:
			ui_.edtActionParam->show();
			break;
		case 7:
			ui_.cmbActionParam->clear();
			ui_.cmbActionParam->addItems(pa_->groupList());
			ui_.cmbActionParam->show();
			break;
	}
}

void ContactManagerDlg::changeDomain(QList<UserListItem *>& users)
{
	QString domain = ui_.edtActionParam->text();
	if (domain.size()) {
		um->startBatch();
		um->clear();
		foreach (UserListItem *u, users) {
			JT_Roster *r = new JT_Roster(pa_->client()->rootTask());
			if (!u->jid().node().isEmpty()) {
				r->set(u->jid().withDomain(domain), u->name(), u->groups());
				r->remove(u->jid());
			}
			r->go(true);
		}
		pa_->client()->rosterRequest();
	} else {
		QMessageBox::warning(this, tr("Invalid"), tr("Please fill parameter field with new domain name"));
	}
}

void ContactManagerDlg::changeGroup(QList<UserListItem *>& users)
{
	QStringList groups(ui_.cmbActionParam->currentText());

	foreach (UserListItem *u, users) {
		JT_Roster *r = new JT_Roster(pa_->client()->rootTask());
		r->set(u->jid(), u->name(), groups);
		r->go(true);
	}
}

void ContactManagerDlg::client_rosterUpdated(bool success, int statusCode,QString statusString)
{
	if (success) {
		um->reloadUsers();
	}
	ui_.usersView->viewport()->update();
	Q_UNUSED(statusCode);
	Q_UNUSED(statusString);
	um->stopBatch();
}

void ContactManagerDlg::exportRoster(QList<UserListItem *>& users)
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Roster file"), QDir::homePath());
	if (!fileName.isEmpty()) {
		QDomDocument doc;
		QString nick;
		QDomElement root = doc.createElement("roster");
		doc.appendChild(root);
		foreach (UserListItem *u, users) {
			QDomElement contact = root.appendChild(doc.createElement("contact")).toElement();
			contact.setAttribute("jid", u->jid().bare());
			foreach (QString group, u->groups()) {
				contact.appendChild(doc.createElement("group")).appendChild(doc.createTextNode(group));
			}
			nick = u->name();
			if (!nick.isEmpty()) {
				contact.appendChild(doc.createElement("nick")).appendChild(doc.createTextNode(nick));
			}
		}
		QFile file(fileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QMessageBox::critical(this, tr("Save error!"), tr("Can't open file %1 for writing").arg(fileName));
			return;
		}
		file.write(doc.toString().toLocal8Bit());
		file.close();
	}
}

void ContactManagerDlg::importRoster()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Roster file"), QDir::homePath());
	if (!fileName.isEmpty()) {
		QFile file(fileName);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QMessageBox::critical(this, tr("Open error!"), tr("Can't open file %1 for reading").arg(fileName));
			return;
		}
		QDomDocument doc;
		if (!doc.setContent(&file)) {
			QMessageBox::critical(this, tr("Open error!"), tr("File %1 is not xml file").arg(fileName));
			return;
		}
		QDomNodeList domContacts = doc.documentElement().elementsByTagName("contact");
		if (domContacts.isEmpty()) {
			QMessageBox::warning(this, tr("Nothing to do.."), tr("No contacts found in file %1").arg(fileName));
		}

		QHash<QString, QString>nicks;
		QHash<QString, QStringList>groups;
		QString jid, nick;
		QStringList jids;
		QStringList labelContent;
		for (int i = 0; i < static_cast<int>(domContacts.length()); i++) {
			QDomElement contact = domContacts.item(i).toElement();
			jid = contact.attribute("jid");
			jids.append(jid);
			QDomNodeList props = contact.childNodes();
			for (int j = 0; j < static_cast<int>(props.length()); j++) {
				if (!props.item(j).isElement()) {
					continue;
				}
				QDomElement el = props.item(j).toElement();
				if (el.tagName() == "nick") {
					nicks[jid] = el.text();
				}
				if (el.tagName() == "group") {
					groups[jid].append(el.text());
				}
			}
			labelContent.append(QString("%1 (%2)").arg(jid).arg(nicks[jid]));
		}

		QMessageBox confirmDlg(
				QMessageBox::Question,tr("Confirm contacts importing"),
				tr("Do you really want to import these contacts?"), QMessageBox::Cancel | QMessageBox::Yes);
		confirmDlg.setDetailedText(labelContent.join("\n"));
		if (confirmDlg.exec() == QMessageBox::Yes) {
			um->startBatch();
			um->clear();
			foreach (QString jid, jids) {
				JT_Roster *r = new JT_Roster(pa_->client()->rootTask());
				r->set(Jid(jid), nicks[jid], groups[jid]);
				r->go(true);
			}
			pa_->client()->rosterRequest(); // через ж.., но пускай пока так.
		}
		file.close();
	}
}
