/*
 * activitydlg.cpp
 * Copyright (C) 2008 Armando Jagucki
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

#include "xmpp_pubsubitem.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "activitydlg.h"
#include "activitycatalog.h"
#include "psiaccount.h"
#include "pepmanager.h"

ActivityDlg::ActivityDlg(PsiAccount* pa) : QDialog(0), pa_(pa)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.cb_general_type, SIGNAL(currentIndexChanged(const QString&)), SLOT(loadSpecificActivities(const QString&)));
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setActivity()));

	ui_.cb_general_type->addItem(tr("<unset>"));
	foreach(ActivityCatalog::Entry e, ActivityCatalog::instance()->entries()) {
		if (e.specificType() == Activity::UnknownSpecific) {
			// The entry e is for a 'general' type.
			ui_.cb_general_type->addItem(e.text());
		}
	}
}

void ActivityDlg::loadSpecificActivities(const QString& generalActivityStr)
{
	ui_.cb_specific_type->clear();

	if (generalActivityStr == tr("<unset>")) {
		return;
	}
	else {
		ui_.cb_specific_type->addItem(tr("<unset>"));
		ActivityCatalog* ac = ActivityCatalog::instance();
		foreach(ActivityCatalog::Entry e, ac->entries()) {
			if (e.specificType() != Activity::UnknownSpecific) {
				// The entry e is for a 'specific' type.
				ActivityCatalog::Entry ge = ac->findEntryByText(generalActivityStr);
				if (e.type() == ge.type()) {
					ui_.cb_specific_type->addItem(e.text());
				}
			}
		}
	}

	ui_.cb_specific_type->addItem(tr("Other"));
}

void ActivityDlg::setActivity()
{
	QString generalActivityStr  = ui_.cb_general_type->currentText();
	QString specificActivityStr = ui_.cb_specific_type->currentText();

	if (generalActivityStr == tr("<unset>")) {
		pa_->pepManager()->retract("http://jabber.org/protocol/activity", "current");
	}
	else {
		ActivityCatalog* ac = ActivityCatalog::instance();
		Activity::Type generalType = ac->findEntryByText(generalActivityStr).type();

		Activity::SpecificType specificType = Activity::UnknownSpecific;
		if (specificActivityStr != tr("<unset>")) {
			specificType = ac->findEntryByText(specificActivityStr).specificType();
		}
		pa_->pepManager()->publish("http://jabber.org/protocol/activity", PubSubItem("current",Activity(generalType,specificType,ui_.le_description->text()).toXml(*pa_->client()->rootTask()->doc())), PEPManager::PresenceAccess);
	}
	close();
}
