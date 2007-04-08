/*
 * statusdlg.cpp - dialogs for setting and reading status messages
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "statusdlg.h"

#include <qpushbutton.h>
#include <QMessageBox>
#include <qlayout.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qinputdialog.h>
#include <qcheckbox.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

#include "jidutil.h"
#include "psicon.h"
#include "psiaccount.h"
#include "userlist.h"
#include "common.h"
#include "msgmle.h"
#include "statuspreset.h"
#include "shortcutmanager.h"


//----------------------------------------------------------------------------
// StatusShowDlg
// FIXME: Will no longer be needed once it is out of the groupchat contactview
//----------------------------------------------------------------------------
StatusShowDlg::StatusShowDlg(const UserListItem &u)
:QDialog(0, 0, false, Qt::WDestructiveClose)
{
	// build the dialog
	QVBoxLayout *vb = new QVBoxLayout(this, 8);
	PsiTextView *te = new PsiTextView(this);
	vb->addWidget(te);
	QHBoxLayout *hb = new QHBoxLayout(vb);
	QPushButton *pb = new QPushButton(tr("&Close"), this);
	connect(pb, SIGNAL(clicked()), SLOT(close()));
	hb->addStretch(1);
	hb->addWidget(pb);
	hb->addStretch(1);

	// set the rest up
	te->setReadOnly(true);
	te->setTextFormat(Qt::RichText);
	te->setText(u.makeDesc());

	setWindowTitle(tr("Status for %1").arg(JIDUtil::nickOrJid(u.name(), u.jid().full())));
	resize(400,240);

	pb->setFocus();
}


//----------------------------------------------------------------------------
// StatusSetDlg
//----------------------------------------------------------------------------
static XMPP::Status::Type combomap[7] = { STATUS_CHAT, STATUS_ONLINE, STATUS_AWAY, STATUS_XA, STATUS_DND, STATUS_INVISIBLE, STATUS_OFFLINE };

class StatusSetDlg::Private
{
public:
	Private() {}

	PsiCon *psi;
	PsiAccount *pa;
	Status s;
	ChatView *te;
	QComboBox *cb_type, *cb_preset;
	QLineEdit *le_priority;
	QCheckBox *save;
};

StatusSetDlg::StatusSetDlg(PsiCon *psi, const Status &s)
:QDialog(0, 0, false, Qt::WDestructiveClose)
{
	d = new Private;
	d->psi = psi;
	d->pa = 0;
	d->psi->dialogRegister(this);
	d->s = s;

	setWindowTitle(CAP(tr("Set Status: All accounts")));
	init();
}

StatusSetDlg::StatusSetDlg(PsiAccount *pa, const Status &s)
:QDialog(0, 0, false, Qt::WDestructiveClose)
{
	d = new Private;
	d->psi = 0;
	d->pa = pa;
	d->pa->dialogRegister(this);
	d->s = s;

	setWindowTitle(CAP(tr("Set Status: %1").arg(d->pa->name())));
	init();
}

void StatusSetDlg::init()
{
	int type = makeSTATUS(d->s);

	// build the dialog
	QVBoxLayout *vb = new QVBoxLayout(this, 8);
	QHBoxLayout *hb1 = new QHBoxLayout(vb);

	// Status
	QLabel *l;
	l = new QLabel(tr("Status:"), this);
	hb1->addWidget(l);
	d->cb_type = new QComboBox(this);
	int n;
	for(n = 0; n < 7; ++n)
		d->cb_type->insertItem(status2txt(combomap[n]));
	for(n = 0; n < 7; ++n) {
		if(type == combomap[n]) {
			d->cb_type->setCurrentItem(n);
			break;
		}
	}
	hb1->addWidget(d->cb_type,3);

	// Priority
	l = new QLabel(tr("Priority:"), this);
	hb1->addWidget(l);
	d->le_priority = new QLineEdit(this);
	d->le_priority->setMinimumWidth(30);
	hb1->addWidget(d->le_priority,1);
	
	// Status preset
	l = new QLabel(tr("Preset:"), this);
	hb1->addWidget(l);
	d->cb_preset = new QComboBox(this);
	d->cb_preset->insertItem(tr("<None>"));
	QStringList presets;
	foreach(StatusPreset p, option.sp) {
		presets += p.name();
	}
	presets.sort();
	d->cb_preset->insertStringList(presets);
	connect(d->cb_preset, SIGNAL(highlighted(int)), SLOT(chooseStatusPreset(int)));
	hb1->addWidget(d->cb_preset,3);

	d->te = new ChatView(this,this);
	d->te->setReadOnly(false);
	d->te->setTextFormat(Qt::PlainText);
	d->te->setMinimumHeight(50);
	vb->addWidget(d->te);
	QHBoxLayout *hb = new QHBoxLayout(vb);
	QPushButton *pb1 = new QPushButton(tr("&Set"), this);
	QPushButton *pb2 = new QPushButton(tr("&Cancel"), this);
	d->save = new QCheckBox(this);
	d->save->setText(tr("Sa&ve as Preset"));
	d->save->setChecked(false);
	hb->addWidget(pb1);
	hb->addStretch(1);
	hb->addWidget(d->save);
	hb->addStretch(1);
	hb->addWidget(pb2);

	// set the rest up
	d->te->setTextFormat(Qt::PlainText);
	d->te->setText(d->s.status());
	d->te->selectAll();
	connect(pb1, SIGNAL(clicked()), SLOT(doButton()));
	connect(pb2, SIGNAL(clicked()), SLOT(cancel()));
	d->te->setFocus();
	
	ShortcutManager::connect("common.close", this, SLOT(close()));
	ShortcutManager::connect("status.set", this, SLOT(doButton()));

	resize(400,240);
}

StatusSetDlg::~StatusSetDlg()
{
	if(d->psi)
		d->psi->dialogUnregister(this);
	else if(d->pa)
		d->pa->dialogUnregister(this);
	delete d;
}

void StatusSetDlg::doButton()
{
	// Save preset
	if (d->save->isChecked()) {
		QString text;
		while(1) {
			// Get preset
			bool ok = FALSE;
			text = QInputDialog::getText(
				CAP(tr("New Status Preset")),
					tr("Please enter a name for the new status preset:"),
					QLineEdit::Normal, text, &ok, this);
			if (!ok)
				return;
			
			// Check preset name
			if (text.isEmpty()) {
				QMessageBox::information(this, tr("Error"), 
					tr("Can't create a blank preset!"));
			}
			else if(option.sp.contains(text)) {
				QMessageBox::information(this, tr("Error"), 
					tr("You already have a preset with that name!"));
			}
			else 
				break;
		}
		// Store preset
 		option.sp[text] = StatusPreset(text,d->te->text());
 		if (!d->le_priority->text().isEmpty()) 
 			option.sp[text].setPriority(d->le_priority->text().toInt());
 		option.sp[text].setStatus(combomap[d->cb_type->currentItem()]);
	} 

	// Set status
	int type = combomap[d->cb_type->currentItem()];
	QString str = d->te->text();

 	if (d->le_priority->text().isEmpty())
 		emit set(makeStatus(type, str), false);
 	else 
 		emit set(makeStatus(type, str, d->le_priority->text().toInt()), true);

	close();
}

void StatusSetDlg::chooseStatusPreset(int x)
{
	if(x < 1)
		return;
	
	StatusPreset preset = option.sp[d->cb_preset->text(x)];
	d->te->setText(preset.message());
	if (preset.priority().hasValue()) 
		d->le_priority->setText(QString::number(preset.priority().value()));
	else
		d->le_priority->clear();

	int n;
	for(n = 0; n < 7; ++n) {
		if(preset.status() == combomap[n]) {
			d->cb_type->setCurrentItem(n);
			break;
		}
	}
}

void StatusSetDlg::cancel()
{
	emit cancelled();
	close();
}

void StatusSetDlg::reject()
{
	cancel();
	QDialog::reject();
}
