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
#include "psiiconset.h"
#include "psiaccount.h"
#include "userlist.h"
#include "common.h"
#include "psitextview.h"
#include "msgmle.h"
#include "statuspreset.h"
#include "statuscombobox.h"
#include "shortcutmanager.h"
#include "priorityvalidator.h"

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
	bool withPriority;
	ChatEdit *te;
	StatusComboBox *cb_type;
	QComboBox *cb_preset;
	QLineEdit *le_priority;
	QCheckBox *save;
	Jid j;
	QList<XMPP::Jid> jl;
	setStatusEnum setStatusMode;
};

StatusSetDlg::StatusSetDlg(PsiCon *psi, const Status &s, bool withPriority)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->psi = psi;
	d->pa = 0;
	d->psi->dialogRegister(this);
	d->s = s;
	d->withPriority = withPriority;

	setWindowTitle(CAP(tr("Set Status: All accounts")));
	d->setStatusMode = setStatusForAccount;
	init();
}

StatusSetDlg::StatusSetDlg(PsiAccount *pa, const Status &s, bool withPriority)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->psi = 0;
	d->pa = pa;
	d->pa->dialogRegister(this);
	d->s = s;
	d->withPriority = withPriority;

	setWindowTitle(CAP(tr("Set Status: %1").arg(d->pa->name())));
	d->setStatusMode = setStatusForAccount;
	init();
}

void StatusSetDlg::setJid(const Jid &j)
{
	d->j = j;
	setWindowTitle(CAP(tr("Set Status for %1").arg(j.full())));
	d->setStatusMode = setStatusForJid;
}

void StatusSetDlg::setJidList(const QList<XMPP::Jid> &jl)
{
	d->jl = jl;
	setWindowTitle(CAP(tr("Set Status for group")));
	d->setStatusMode = setStatusForJidList;
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
	PriorityValidator* prValidator = new PriorityValidator(d->le_priority);
	d->le_priority->setValidator(prValidator);
	hb1->addWidget(d->le_priority,1);

	// Status preset
	l = new QLabel(tr("Preset:"), this);
	hb1->addWidget(l);
	d->cb_preset = new QComboBox(this);
	d->cb_preset->addItem(tr("<None>"));
	foreach(QVariant name, PsiOptions::instance()->mapKeyList("options.status.presets", true)) {
		StatusPreset sp;
		sp.fromOptions(PsiOptions::instance(), name.toString());
		sp.filterStatus();
#ifdef Q_OS_MAC
		d->cb_preset->addItem(sp.name());
#else
		d->cb_preset->addItem(PsiIconset::instance()->status(sp.status()).icon(), sp.name());
#endif

	}
	connect(d->cb_preset, SIGNAL(currentIndexChanged(int)), SLOT(chooseStatusPreset(int)));
	hb1->addWidget(d->cb_preset,3);

	d->te = new ChatEdit(this);
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
	d->te->setPlainText(d->s.status());
	d->te->selectAll();
	if (d->withPriority) {
		d->le_priority->setText(QString::number(d->s.priority()));
	}
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
	d->te->setPlainText(d->te->toPlainText().trimmed());

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
		StatusPreset sp(text, d->te->toPlainText(), XMPP::Status(d->cb_type->status()).type());
 		if (!d->le_priority->text().isEmpty()) {
			sp.setPriority(d->le_priority->text().toInt());
		}

		sp.toOptions(PsiOptions::instance());
		PsiOptions::instance()->mapPut("options.status.presets", text);

		//PsiCon will emit signal to refresh presets in all status menus
		(d->psi ? d->psi : d->pa->psi())->updateStatusPresets();
	}

	// Set status
	int type = d->cb_type->status();
	QString str = d->te->toPlainText();

	PsiOptions::instance()->setOption("options.status.last-priority", d->le_priority->text());
	PsiOptions::instance()->setOption("options.status.last-message", str);
	PsiOptions::instance()->setOption("options.status.last-status", XMPP::Status(d->cb_type->status()).typeString());

	if (d->le_priority->text().isEmpty())
		switch(d->setStatusMode) {
			case setStatusForAccount:
				emit set(makeStatus(type,str), false, true);
				break;
			case setStatusForJid:
				emit setJid(d->j, makeStatus(type,str));
				break;
			case setStatusForJidList:
				emit setJidList(d->jl, makeStatus(type,str));
				break;
		}
	else {
		switch(d->setStatusMode) {
			case setStatusForAccount:
				emit set(makeStatus(type,str, d->le_priority->text().toInt()), true, true);
				break;
			case setStatusForJid:
				emit setJid(d->j, makeStatus(type,str, d->le_priority->text().toInt()));
				break;
			case setStatusForJidList:
				emit setJidList(d->jl, makeStatus(type,str, d->le_priority->text().toInt()));
				break;
		}
	}
	close();
}

void StatusSetDlg::chooseStatusPreset(int x)
{
	if(x < 1)
		return;

	QString base = PsiOptions::instance()->mapLookup("options.status.presets", d->cb_preset->itemText(x));
	d->te->setPlainText(PsiOptions::instance()->getOption(base+".message").toString());
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
