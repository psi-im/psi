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
#include "psiiconset.h"

ActivityDlg::ActivityDlg(QList<PsiAccount*> list) : QDialog(0), pa_(list)
{
	setAttribute(Qt::WA_DeleteOnClose);
	if(pa_.isEmpty())
		close();
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.cb_general_type, SIGNAL(currentIndexChanged(const QString&)), SLOT(loadSpecificActivities(const QString&)));
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setActivity()));

	ui_.cb_general_type->addItem(tr("<unset>"));
	PsiAccount* pa = pa_.first();
	Activity::Type at = pa->activity().type();
	int i=1;
	foreach(ActivityCatalog::Entry e, ActivityCatalog::instance()->entries()) {
		if (e.specificType() == Activity::UnknownSpecific) {
			// The entry e is for a 'general' type.
			ui_.cb_general_type->addItem(IconsetFactory::icon("activities/"+e.value()).icon(), e.text());
			if (e.type() == at) {
				ui_.cb_general_type->setCurrentIndex(i);
				loadSpecificActivities(ui_.cb_general_type->currentText());
			}
			i++;
		}
	}
	ui_.le_description->setText(pa->activity().text());
}

void ActivityDlg::loadSpecificActivities(const QString& generalActivityStr)
{
	ui_.cb_specific_type->clear();

	if (generalActivityStr == tr("<unset>")) {
		return;
	}
	else {
		ui_.cb_specific_type->addItem(tr("<unset>"));
		PsiAccount* pa = pa_.first();
		Activity::SpecificType at = pa->activity().specificType();
		int i=1;
		ActivityCatalog* ac = ActivityCatalog::instance();
		foreach(ActivityCatalog::Entry e, ac->entries()) {
			if (e.specificType() != Activity::UnknownSpecific) {
				// The entry e is for a 'specific' type.
				ActivityCatalog::Entry ge = ac->findEntryByText(generalActivityStr);
				if (e.type() == ge.type()) {
					ui_.cb_specific_type->addItem(IconsetFactory::icon("activities/"+ge.value()+"_"+e.value()).icon(), e.text());
					if (e.specificType() == at) {
						ui_.cb_specific_type->setCurrentIndex(i);
					}
					i++;
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

	foreach(PsiAccount *pa, pa_) {
		if (generalActivityStr == tr("<unset>")) {
			pa->pepManager()->retract("http://jabber.org/protocol/activity", "current");
		}
		else {
			ActivityCatalog* ac = ActivityCatalog::instance();
			Activity::Type generalType = ac->findEntryByText(generalActivityStr).type();

			Activity::SpecificType specificType = Activity::UnknownSpecific;
			if (specificActivityStr != tr("<unset>")) {
				specificType = ac->findEntryByText(specificActivityStr).specificType();
			}
			pa->pepManager()->publish("http://jabber.org/protocol/activity", PubSubItem("current",Activity(generalType,specificType,ui_.le_description->text()).toXml(*pa->client()->rootTask()->doc())), PEPManager::PresenceAccess);
		}
	}
	close();
}
