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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mooddlg.h"
#include "moodcatalog.h"
#include "psiaccount.h"
#include "pepmanager.h"
#include "im.h"

MoodDlg::MoodDlg(PsiAccount* pa)
	: QDialog(0), pa_(pa)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);
	setModal(false);
	connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setMood()));	

	foreach(MoodCatalog::Entry e, MoodCatalog::instance()->entries()) {
		ui_.cb_type->addItem(e.text());
	}
}


void MoodDlg::setMood()
{
	Mood::Type type = MoodCatalog::instance()->findEntryByText(ui_.cb_type->currentText()).type();
	pa_->pepManager()->publish("http://jabber.org/protocol/mood", PubSubItem("current",Mood(type,ui_.le_text->text()).toXml(*pa_->client()->rootTask()->doc())));
	close();
}
