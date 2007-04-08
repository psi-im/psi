#include "opt_status.h"
#include "common.h"
#include "iconwidget.h"

#include <qbuttongroup.h>
#include <QMessageBox>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qtextedit.h>
#include <qinputdialog.h>

#include "ui_opt_status.h"

class OptStatusUI : public QWidget, public Ui::OptStatus
{
public:
	OptStatusUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabStatus
//----------------------------------------------------------------------------

static int combomap[7] = { STATUS_CHAT, STATUS_ONLINE, STATUS_AWAY, STATUS_XA, STATUS_DND, STATUS_INVISIBLE, STATUS_OFFLINE };

OptionsTabStatus::OptionsTabStatus(QObject *parent)
: OptionsTab(parent, "status", "", tr("Status"), tr("Status preferences"), "psi/status")
{
	w = 0;
	o = new Options;
}

OptionsTabStatus::~OptionsTabStatus()
{
	delete o;
}

QWidget *OptionsTabStatus::widget()
{
	if ( w )
		return 0;

	w = new OptStatusUI();
	OptStatusUI *d = (OptStatusUI *)w;

	QString s = tr("Makes Psi automatically set your status to \"away\" if your"
		" computer is idle for the specified amount of time.");
	QWhatsThis::add(d->ck_asAway, s);
	QWhatsThis::add(d->sb_asAway, s);
	s = tr("Makes Psi automatically set your status to \"extended away\" if your"
		" computer is idle for the specified amount of time.");
	QWhatsThis::add(d->ck_asXa, s);
	QWhatsThis::add(d->sb_asXa, s);
	s = tr("Makes Psi automatically set your status to \"offline\" if your"
		" computer is idle for the specified amount of time."
		"  This will disconnect you from the Jabber server.");
	QWhatsThis::add(d->ck_asOffline, s);
	QWhatsThis::add(d->sb_asOffline, s);

	QWhatsThis::add(d->te_asMessage,
		tr("Specifies an extended message to use if you allow Psi"
		" to set your status automatically.  See options above."));

	d->pb_spNew->setEnabled(TRUE);
	d->pb_spDelete->setEnabled(FALSE);
	d->te_sp->setEnabled(FALSE);
	connect(d->pb_spNew, SIGNAL(clicked()), SLOT(newStatusPreset()));
	connect(d->pb_spDelete, SIGNAL(clicked()), SLOT(removeStatusPreset()));
	connect(d->cb_preset, SIGNAL(highlighted(int)), SLOT(selectStatusPreset(int)));
	connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
	connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
	connect(d->cb_sp_status, SIGNAL(activated(int)), SLOT(changeStatusPreset()));
	
	int n;
	for(n = 0; n < 7; ++n)
		d->cb_sp_status->insertItem(status2txt(combomap[n]));

	QWhatsThis::add(d->pb_spNew,
		tr("Press this button to create a new status message preset."));
	QWhatsThis::add(d->pb_spDelete,
		tr("Press this button to delete a status message preset."));
	QWhatsThis::add(d->cb_preset,
		tr("Use this list to select a status message preset"
		" to view or edit in the box to the bottom."));
	QWhatsThis::add(d->te_sp,
		tr("You may edit the message here for the currently selected"
		" status message preset in the list to the above."));
	QWhatsThis::add(d->cb_sp_status,
		tr("Use this to choose the status that will be assigned to this preset"));
	QWhatsThis::add(d->le_sp_priority,
		tr("Fill in the priority that will be assigned to this preset."
		   " If no priority is given, the default account priority will be used."));

	QWhatsThis::add(d->ck_askOnline,
		tr("Jabber allows you to put extended status messages on"
		" all status types.  Normally, Psi does not prompt you for"
		" an extended message when you set your status to \"online\"."
		"  Check this option if you want to have this prompt."));

	return w;
}

void OptionsTabStatus::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptStatusUI *d = (OptStatusUI *)w;

	opt->asAway = d->sb_asAway->value();
	opt->asXa = d->sb_asXa->value();
	opt->asOffline = d->sb_asOffline->value();
	opt->use_asAway = d->ck_asAway->isChecked();
	opt->use_asXa = d->ck_asXa->isChecked();
	opt->use_asOffline = d->ck_asOffline->isChecked();
	opt->asMessage = d->te_asMessage->text();

	opt->sp = o->sp;

	opt->askOnline = d->ck_askOnline->isChecked();
	opt->askOffline = d->ck_askOffline->isChecked();
}

void OptionsTabStatus::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptStatusUI *d = (OptStatusUI *)w;

	d->sb_asAway->setMinValue(0);
	d->sb_asAway->setValue( opt->asAway );
	d->sb_asXa->setMinValue(0);
	d->sb_asXa->setValue( opt->asXa );
	d->sb_asOffline->setMinValue(0);
	d->sb_asOffline->setValue( opt->asOffline );
	/*if (opt->asAway <= 0 )
		opt->use_asAway = FALSE;
	if (opt->asXa <= 0 )
		opt->use_asXa = FALSE;
	if(d->opt.asOffline <= 0)
		opt->use_asOffline = FALSE;*/
	d->ck_asAway->setChecked( opt->use_asAway );
	d->ck_asXa->setChecked( opt->use_asXa );
	d->ck_asOffline->setChecked( opt->use_asOffline );
	d->te_asMessage->setText( opt->asMessage );

	o->sp = opt->sp;
	d->cb_preset->clear();
	QStringList presets;
	foreach(StatusPreset p, option.sp) {
		presets += p.name();
	}
	d->cb_preset->insertStringList(presets);

	if(d->cb_preset->count() >= 1) {
		d->cb_preset->setCurrentIndex(0);
		selectStatusPreset(0);
	}

	d->ck_askOnline->setChecked( opt->askOnline );
	d->ck_askOffline->setChecked( opt->askOffline );
}

void OptionsTabStatus::setData(PsiCon *, QWidget *parentDialog)
{
	parentWidget = parentDialog;
}

void OptionsTabStatus::selectStatusPreset(int x)
{
	OptStatusUI *d = (OptStatusUI *)w;

	//noDirty = TRUE;
	disconnect(d->te_sp, SIGNAL(textChanged()), 0, 0);
	disconnect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), 0, 0);
	if ( x == -1 ) {
		d->pb_spDelete->setEnabled(false);
		d->te_sp->setText("");
		d->te_sp->setEnabled(false);
		d->le_sp_priority->clear();
		d->le_sp_priority->setEnabled(false);
		d->cb_sp_status->setEnabled(false);

		//noDirty = FALSE;
		connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
		connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
		return;
	}

	d->pb_spDelete->setEnabled(true);

	StatusPreset preset = o->sp[d->cb_preset->text(x)];
	d->te_sp->setText(preset.message());
	d->te_sp->setEnabled(true);
	if (preset.priority().hasValue())
		d->le_sp_priority->setText(QString::number(preset.priority().value()));
	else
		d->le_sp_priority->clear();
	d->le_sp_priority->setEnabled(true);
	int n;
	for(n = 0; n < 7; ++n) {
		if(preset.status() == combomap[n]) {
			d->cb_sp_status->setCurrentItem(n);
			break;
		}
	}
	d->cb_sp_status->setEnabled(true);

	//noDirty = FALSE;
	connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
	connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
}

void OptionsTabStatus::newStatusPreset()
{
	OptStatusUI *d = (OptStatusUI *)w;

	QString text;

	while(1) {
		bool ok = FALSE;
		text = QInputDialog::getText(
			CAP(tr("New Status Preset")),
			tr("Please enter a name for the new status preset:"),
			QLineEdit::Normal, text, &ok, parentWidget);
		if(!ok)
			return;

		if(text.isEmpty())
			QMessageBox::information(parentWidget, tr("Error"), tr("Can't create a blank preset!"));
		else if(o->sp.contains(text))
			QMessageBox::information(parentWidget, tr("Error"), tr("You already have a preset with that name!"));
		else
			break;
	}

	o->sp[text].setName(text);
	d->cb_preset->insertItem(text);
	d->cb_preset->setCurrentItem(d->cb_preset->count()-1);
	selectStatusPreset(d->cb_preset->count()-1);
	d->te_sp->setFocus();

	emit dataChanged();
}

void OptionsTabStatus::removeStatusPreset()
{
	OptStatusUI *d = (OptStatusUI *)w;
	int id = d->cb_preset->currentItem();
	if(id == -1)
		return;

	emit dataChanged();

	o->sp.remove(d->cb_preset->text(id));
	d->cb_preset->removeItem(id);

	// select a new entry if possible
	if(d->cb_preset->count() == 0) {
		selectStatusPreset(-1);
		return;
	}

	if(id >= (int)d->cb_preset->count())
		id = d->cb_preset->count()-1;

	d->cb_preset->setCurrentIndex(id);
	selectStatusPreset(id);
}

void OptionsTabStatus::changeStatusPreset()
{
	OptStatusUI *d = (OptStatusUI *)w;
	int id = d->cb_preset->currentItem();
	if(id == -1)
		return;

	o->sp[d->cb_preset->text(id)].setMessage(d->te_sp->text());
	if (d->le_sp_priority->text().isEmpty())
		o->sp[d->cb_preset->text(id)].clearPriority();
	else
		o->sp[d->cb_preset->text(id)].setPriority(d->le_sp_priority->text().toInt());
	o->sp[d->cb_preset->text(id)].setStatus(combomap[d->cb_sp_status->currentItem()]);

	emit dataChanged();
}
