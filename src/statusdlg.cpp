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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "statusdlg.h"

#include <QPushButton>
#include <QMessageBox>
#include <QLayout>
#include <QLabel>
#include <QComboBox>
#include <QInputDialog>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

#include "jidutil.h"
#include "psicon.h"
#include "psioptions.h"
#include "psiaccount.h"
#include "userlist.h"
#include "common.h"
#include "msgmle.h"
#include "statuspreset.h"
#include "statuscombobox.h"
#include "shortcutmanager.h"


//----------------------------------------------------------------------------
// StatusShowDlg
// FIXME: Will no longer be needed once it is out of the groupchat contactview
//----------------------------------------------------------------------------
StatusShowDlg::StatusShowDlg(const UserListItem &u)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	// build the dialog
	QVBoxLayout *vb = new QVBoxLayout(this);
	vb->setMargin(8);
	PsiTextView *te = new PsiTextView(this);
	vb->addWidget(te);
	QHBoxLayout *hb = new QHBoxLayout;
	vb->addLayout(hb);
	QPushButton *pb = new QPushButton(tr("&Close"), this);
	connect(pb, SIGNAL(clicked()), SLOT(close()));
	hb->addStretch(1);
	hb->addWidget(pb);
	hb->addStretch(1);

	// set the rest up
	te->setReadOnly(true);
	te->setAcceptRichText(true);
	te->setText(u.makeDesc());

	setWindowTitle(tr("Status for %1").arg(JIDUtil::nickOrJid(u.name(), u.jid().full())));
	resize(400,240);

	pb->setFocus();
}


//----------------------------------------------------------------------------
// StatusSetDlg
//----------------------------------------------------------------------------

class StatusSetDlg::Private
{
public:
	Private() {}

	PsiCon *psi;
	PsiAccount *pa;
	Status s;
	ChatView *te;
	StatusComboBox *cb_type;
	QComboBox *cb_preset;
	QLineEdit *le_priority;
	QCheckBox *save;
};

StatusSetDlg::StatusSetDlg(PsiCon *psi, const Status &s)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->psi = psi;
	d->pa = 0;
	d->psi->dialogRegister(this);
	d->s = s;

	setWindowTitle(CAP(tr("Set Status: All accounts")));
	init();
}

StatusSetDlg::StatusSetDlg(PsiAccount *pa, const Status &s)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
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
	QVBoxLayout *vb = new QVBoxLayout(this);
	vb->setMargin(8);
	QHBoxLayout *hb1 = new QHBoxLayout;
	vb->addLayout(hb1);

	// Status
	QLabel *l;
	l = new QLabel(tr("Status:"), this);
	hb1->addWidget(l);
	d->cb_type = new StatusComboBox(this, static_cast<XMPP::Status::Type>(type));
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
	d->cb_preset->addItem(tr("<None>"));
	QStringList presets;
	foreach(QVariant name, PsiOptions::instance()->mapKeyList("options.status.presets")) {
		presets += name.toString();
	}
	presets.sort();
	d->cb_preset->addItems(presets);
	connect(d->cb_preset, SIGNAL(currentIndexChanged(int)), SLOT(chooseStatusPreset(int)));
	hb1->addWidget(d->cb_preset,3);

	d->te = new ChatView(this);
	d->te->setDialog(this);
	d->te->setReadOnly(false);
	d->te->setAcceptRichText(false);
	d->te->setMinimumHeight(50);
	vb->addWidget(d->te);
	QHBoxLayout *hb = new QHBoxLayout;
	vb->addLayout(hb);
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
	d->te->setAcceptRichText(false);
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
	// Trim whitespace
	d->te->setText(d->te->getPlainText().trimmed());

	// Save preset
	if (d->save->isChecked()) {
		QString text;
		while(1) {
			// Get preset
			bool ok = false;
			text = QInputDialog::getText(this,
				CAP(tr("New Status Preset")),
					tr("Please enter a name for the new status preset:"),
					QLineEdit::Normal, text, &ok);
			if (!ok)
				return;

			// Check preset name
			if (text.isEmpty()) {
				QMessageBox::information(this, tr("Error"),
					tr("Can't create a blank preset!"));
			}
			else if(PsiOptions::instance()->mapKeyList("options.status.presets").contains(text)) {
				QMessageBox::information(this, tr("Error"),
					tr("You already have a preset with that name!"));
			}
			else
				break;
		}
		// Store preset
		StatusPreset sp(text, d->te->getPlainText(), XMPP::Status(d->cb_type->status()).type());
 		if (!d->le_priority->text().isEmpty()) {
			sp.setPriority(d->le_priority->text().toInt());
		}

		sp.toOptions(PsiOptions::instance());
		QString base = PsiOptions::instance()->mapPut("options.status.presets", text);
	}

	// Set status
	int type = d->cb_type->status();
	QString str = d->te->getPlainText();

 	if (d->le_priority->text().isEmpty())
 		emit set(makeStatus(type, str), false, true);
 	else
 		emit set(makeStatus(type, str, d->le_priority->text().toInt()), true, true);

	close();
}

void StatusSetDlg::chooseStatusPreset(int x)
{
	if(x < 1)
		return;

	QString base = PsiOptions::instance()->mapLookup("options.status.presets", d->cb_preset->itemText(x));
	d->te->setText(PsiOptions::instance()->getOption(base+".message").toString());
	if (PsiOptions::instance()->getOption(base+".force-priority").toBool()) {
		d->le_priority->setText(QString::number(PsiOptions::instance()->getOption(base+".priority").toInt()));
	} else {
		d->le_priority->clear();
	}

	XMPP::Status status;
	status.setType(PsiOptions::instance()->getOption(base+".status").toString());
	d->cb_type->setStatus(status.type());
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
