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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "mooddlg.h"

#include "moodcatalog.h"
#include "pepmanager.h"
#include "psiaccount.h"
#include "psiiconset.h"
#include "xmpp_client.h"
#include "xmpp_pubsubitem.h"
#include "xmpp_task.h"

MoodDlg::MoodDlg(QList<PsiAccount *> list) : QDialog(nullptr), pa_(list)
{
    setAttribute(Qt::WA_DeleteOnClose);
    if (pa_.isEmpty())
        close();
    ui_.setupUi(this);
    setWindowIcon(IconsetFactory::icon("mood/").icon());
    setModal(false);

    connect(ui_.pb_cancel, SIGNAL(clicked()), SLOT(close()));
    connect(ui_.pb_ok, SIGNAL(clicked()), SLOT(setMood()));

    ui_.cb_type->addItem(tr("<unset>"));
    PsiAccount *pa = pa_.first();
    Mood::Type  mt = pa->mood().type();
    int         i  = 1;
    for (const MoodCatalog::Entry &e : MoodCatalog::instance()->entries()) {
        ui_.cb_type->addItem(IconsetFactory::icon("mood/" + e.value()).icon(), e.text());
        if (e.type() == mt) {
            ui_.cb_type->setCurrentIndex(i);
        }
        i++;
    }
    ui_.le_text->setText(pa->mood().text());
}

void MoodDlg::setMood()
{
    QString moodstr = ui_.cb_type->currentText();
    for (PsiAccount *pa : qAsConst(pa_)) {
        if (moodstr == tr("<unset>")) {
            pa->pepManager()->disable("mood", PEP_MOOD_NS, "current");
        } else {
            Mood::Type type = MoodCatalog::instance()->findEntryByText(moodstr).type();
            pa->pepManager()->publish(
                PEP_MOOD_NS,
                PubSubItem("current", Mood(type, ui_.le_text->text()).toXml(*pa->client()->rootTask()->doc())));
        }
    }
    close();
}
