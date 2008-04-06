#include "opt_status.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"

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

OptionsTabStatus::OptionsTabStatus(QObject *parent)
: OptionsTab(parent, "status", "", tr("Status"), tr("Status preferences"), "psi/status")
{
	w = 0;
}

OptionsTabStatus::~OptionsTabStatus()
{
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
	if (!PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool()) {
		d->ck_asXa->hide();
		d->sb_asXa->hide();
		d->lb_asXa->hide();
	}
	QWhatsThis::add(d->ck_asOffline, s);
	QWhatsThis::add(d->sb_asOffline, s);

	QWhatsThis::add(d->te_asMessage,
		tr("Specifies an extended message to use if you allow Psi"
		" to set your status automatically.  See options above."));

	setStatusPresetWidgetsEnabled(false);
	connect(d->pb_spNew, SIGNAL(clicked()), SLOT(newStatusPreset()));
	connect(d->pb_spDelete, SIGNAL(clicked()), SLOT(removeStatusPreset()));
	connect(d->cb_preset, SIGNAL(currentIndexChanged(int)), SLOT(selectStatusPreset(int)));
	connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
	connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
	connect(d->cb_sp_status, SIGNAL(activated(int)), SLOT(changeStatusPreset()));
	
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

void OptionsTabStatus::applyOptions()
{
	if ( !w )
		return;

	OptStatusUI *d = (OptStatusUI *)w;

	PsiOptions::instance()->setOption("options.status.auto-away.away-after", d->sb_asAway->value());
	PsiOptions::instance()->setOption("options.status.auto-away.not-availible-after", d->sb_asXa->value());
	PsiOptions::instance()->setOption("options.status.auto-away.offline-after", d->sb_asOffline->value());
	PsiOptions::instance()->setOption("options.status.auto-away.use-away", d->ck_asAway->isChecked());
	PsiOptions::instance()->setOption("options.status.auto-away.use-not-availible", d->ck_asXa->isChecked());
	PsiOptions::instance()->setOption("options.status.auto-away.use-offline", d->ck_asOffline->isChecked());
	PsiOptions::instance()->setOption("options.status.auto-away.message", d->te_asMessage->text());

	
	foreach (QString name, deletedPresets) {
		QString base = PsiOptions::instance()->mapLookup("options.status.presets", name);
		PsiOptions::instance()->removeOption(base , true);
	}
	deletedPresets.clear();
	foreach (QString name, dirtyPresets.toList() + newPresets.keys()) {
		StatusPreset sp;
		if (newPresets.contains(name)) {
			sp = newPresets[name];
		} else {
			sp = presets[name];
		}
		PsiOptions *o = PsiOptions::instance();
		sp.toOptions(o);
	}
	dirtyPresets.clear();
	presets.unite(newPresets);
	newPresets.clear();
	
	PsiOptions::instance()->setOption("options.status.ask-for-message-on-online", d->ck_askOnline->isChecked());
	PsiOptions::instance()->setOption("options.status.ask-for-message-on-offline", d->ck_askOffline->isChecked());
}

void OptionsTabStatus::restoreOptions()
{
	if ( !w )
		return;

	OptStatusUI *d = (OptStatusUI *)w;

	d->sb_asAway->setMinValue(0);
	d->sb_asAway->setValue( PsiOptions::instance()->getOption("options.status.auto-away.away-after").toInt() );
	d->sb_asXa->setMinValue(0);
	d->sb_asXa->setValue( PsiOptions::instance()->getOption("options.status.auto-away.not-availible-after").toInt() );
	d->sb_asOffline->setMinValue(0);
	d->sb_asOffline->setValue( PsiOptions::instance()->getOption("options.status.auto-away.offline-after").toInt() );
	/*if (PsiOptions::instance()->getOption("options.status.auto-away.away-after").toInt() <= 0 )
		PsiOptions::instance()->getOption("options.status.auto-away.use-away").toBool() = FALSE;
	if (PsiOptions::instance()->getOption("options.status.auto-away.not-availible-after").toInt() <= 0 )
		PsiOptions::instance()->getOption("options.status.auto-away.use-not-availible").toBool() = FALSE;
	if(d->opt.asOffline <= 0)
		PsiOptions::instance()->getOption("options.status.auto-away.use-offline").toBool() = FALSE;*/
	d->ck_asAway->setChecked( PsiOptions::instance()->getOption("options.status.auto-away.use-away").toBool() );
	d->ck_asXa->setChecked( PsiOptions::instance()->getOption("options.status.auto-away.use-not-availible").toBool() );
	d->ck_asOffline->setChecked( PsiOptions::instance()->getOption("options.status.auto-away.use-offline").toBool() );
	d->te_asMessage->setText( PsiOptions::instance()->getOption("options.status.auto-away.message").toString() );

	
	QStringList presetNames;
	
	foreach(QVariant name, PsiOptions::instance()->mapKeyList("options.status.presets")) {
		QString base =  PsiOptions::instance()->mapLookup("options.status.presets", name.toString());
		StatusPreset sp;
		sp.setName(name.toString());
		sp.setMessage(PsiOptions::instance()->getOption(base+".message").toString());
		if (PsiOptions::instance()->getOption(base+".force-priority").toBool()) {
			sp.setPriority(PsiOptions::instance()->getOption(base+".priority").toInt());
		}

		XMPP::Status status;
		status.setType(PsiOptions::instance()->getOption(base+".status").toString());
		sp.setStatus(status.type());
		
		presets[name.toString()] = sp;
		presetNames += name.toString();
	}
	
	
	d->cb_preset->insertStringList(presetNames);

	if(d->cb_preset->count() >= 1) {
		d->cb_preset->setCurrentIndex(0);
		selectStatusPreset(0);
	}

	d->ck_askOnline->setChecked( PsiOptions::instance()->getOption("options.status.ask-for-message-on-online").toBool() );
	d->ck_askOffline->setChecked( PsiOptions::instance()->getOption("options.status.ask-for-message-on-offline").toBool() );
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
		setStatusPresetWidgetsEnabled(false);
		d->te_sp->setText("");
		d->le_sp_priority->clear();

		//noDirty = FALSE;
		connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
		connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
		return;
	}

	StatusPreset preset;
	QString name = d->cb_preset->text(x);
	
	if (newPresets.contains(name)) {
		preset = newPresets[name];
	} else {
		preset = presets[name];
	}
	
	d->te_sp->setText(preset.message());
	if (preset.priority().hasValue())
		d->le_sp_priority->setText(QString::number(preset.priority().value()));
	else
		d->le_sp_priority->clear();
	d->cb_sp_status->setStatus(preset.status());
	
	//noDirty = FALSE;
	connect(d->te_sp, SIGNAL(textChanged()), SLOT(changeStatusPreset()));
	connect(d->le_sp_priority, SIGNAL(textChanged(const QString&)), SLOT(changeStatusPreset()));
	
	setStatusPresetWidgetsEnabled(true);
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
		if(!ok) {
			return;
		}

		if(text.isEmpty()) {
			QMessageBox::information(parentWidget, tr("Error"), tr("Can't create a blank preset!"));
		} else if(presets.contains(text) || newPresets.contains(text)) {
			QMessageBox::information(parentWidget, tr("Error"), tr("You already have a preset with that name!"));
		} else {
			break;
		}
	}

	newPresets[text].setName(text);
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

	QString name = d->cb_preset->text(id);
	
	if (newPresets.contains(name)) {
		newPresets.remove(name);
	} else {
		deletedPresets += d->cb_preset->text(id);
		presets.remove(d->cb_preset->text(id));
	}
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

	StatusPreset sp;
	sp.setMessage(d->te_sp->text());
	if (d->le_sp_priority->text().isEmpty())
		sp.clearPriority();
	else
		sp.setPriority(d->le_sp_priority->text().toInt());
	sp.setStatus(d->cb_sp_status->status());

	QString name = d->cb_preset->text(id);
	
	sp.setName(name);
	if (newPresets.contains(name)) {
		newPresets[name] = sp;
	} else {
		dirtyPresets += name;
		presets[name] = sp;
	}
	
	emit dataChanged();
}

void OptionsTabStatus::setStatusPresetWidgetsEnabled(bool enabled)
{
	OptStatusUI *d = (OptStatusUI *)w;
	d->cb_preset->setEnabled(enabled);
	d->pb_spDelete->setEnabled(enabled);
	d->cb_sp_status->setEnabled(enabled);
	d->le_sp_priority->setEnabled(enabled);
	d->te_sp->setEnabled(enabled);
}
