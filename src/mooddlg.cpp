/*
 * mooddlg.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "xmpp_pubsubitem.h"
#include "xmpp_client.h"
#include "xmpp_task.h"
#include "mooddlg.h"
#include "moodcatalog.h"
#include "psiaccount.h"
#include "pepmanager.h"

MoodDlg::MoodDlg(PsiAccount* pa)
	: QDialog(0), pa_(pa)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setMood()));

	ui_.cb_type->addItem(tr("<unset>"));
	foreach(MoodCatalog::Entry e, MoodCatalog::instance()->entries()) {
		ui_.cb_type->addItem(e.text());
	}
}


void MoodDlg::setMood()
{
	QString moodstr = ui_.cb_type->currentText();
	if (moodstr == tr("<unset>")) {
		pa_->pepManager()->retract("http://jabber.org/protocol/mood", "current");
	} else {
		Mood::Type type = MoodCatalog::instance()->findEntryByText(moodstr).type();
		pa_->pepManager()->publish("http://jabber.org/protocol/mood", PubSubItem("current",Mood(type,ui_.le_text->text()).toXml(*pa_->client()->rootTask()->doc())), PEPManager::PresenceAccess);
	}
	close();
}
