#include "opt_statusgeneral.h"
#include "common.h"
#include "psioptions.h"
#include "psiiconset.h"
#include "priorityvalidator.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QWhatsThis>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>

#include "ui_opt_statusgeneral.h"

class OptStatusGeneralUI : public QWidget, public Ui::OptStatusGeneral
{
public:
	OptStatusGeneralUI() : QWidget() { setupUi(this); }
};

OptionsTabStatusGeneral::OptionsTabStatusGeneral(QObject *parent)
	: OptionsTab(parent, "status_general", "", tr("General"), tr("General status preferences"))
	, w(0)
{
}

OptionsTabStatusGeneral::~OptionsTabStatusGeneral()
{
}

QWidget *OptionsTabStatusGeneral::widget()
{
	if ( w )
		return 0;

	w = new OptStatusGeneralUI();
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	connect(d->lw_presets, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), SLOT(currentItemChanged(QListWidgetItem *, QListWidgetItem *)));
	connect(d->lw_presets, SIGNAL(doubleClicked(const QModelIndex &)), SLOT(presetDoubleClicked(const QModelIndex &)));
	connect(d->pb_spNew, SIGNAL(clicked()), SLOT(newStatusPreset()));
	connect(d->pb_spEdit, SIGNAL(clicked()), SLOT(editStatusPreset()));
	connect(d->pb_spDelete, SIGNAL(clicked()), SLOT(deleteStatusPreset()));
	connect(d->cb_presetsMenus, SIGNAL(currentIndexChanged(int)), SLOT(statusMenusIndexChanged(int)));
	connect(d->bb_selPreset, SIGNAL(accepted()), SLOT(statusPresetAccepted()));
	connect(d->bb_selPreset, SIGNAL(rejected()), SLOT(statusPresetRejected()));

	PriorityValidator* prValidator = new PriorityValidator(d->le_sp_priority);
	d->le_sp_priority->setValidator(prValidator);

	spContextMenu = new QMenu(w);
	spContextMenu->addAction(tr("Edit"), this, SLOT(editStatusPreset()));
	spContextMenu->addAction(tr("Delete"), this, SLOT(deleteStatusPreset()));
	connect(d->lw_presets, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showMenuForPreset(const QPoint &)));

	PsiOptions* o = PsiOptions::instance();
	int askCount = 6;
	if (!o->getOption("options.ui.menu.status.chat").toBool()) {
		d->ck_askChat->hide();
		askCount--;
	}
	if (!o->getOption("options.ui.menu.status.xa").toBool()) {
		d->ck_askXa->hide();
		askCount--;
	}

	if (askCount != 6) {
		reorderGridLayout(d->gridLayout, askCount == 4 ? 2 : 3); //4 items in 2 columns look better
	}

	d->pb_spNew->setWhatsThis(
		tr("Press this button to create a new status message preset."));
	d->pb_spDelete->setWhatsThis(
		tr("Press this button to delete a status message preset."));
	/*TODO d->cb_preset->setWhatsThis(
		tr("Use this list to select a status message preset"
		" to view or edit in the box to the right. You can"
		" also sort them manually with drag and drop."));*/
	d->te_sp->setWhatsThis(
		tr("You may edit the message here for the currently selected"
		" status message preset in the list to the above."));
	d->cb_sp_status->setWhatsThis(
		tr("Use this to choose the status that will be assigned to this preset"));
	d->le_sp_priority->setWhatsThis(
		tr("Fill in the priority that will be assigned to this preset."
		   " If no priority is given, the default account priority will be used."));

	d->ck_askOnline->setWhatsThis(
		tr("Jabber allows you to put extended status messages on"
		" all status types.  Normally, Psi does not prompt you for"
		" an extended message when you set your status to \"online\"."
		"  Check this option if you want to have this prompt."));
	//TODO write whatsthis messages for other widgets
	return w;
}

void OptionsTabStatusGeneral::applyOptions()
{
	if ( !w )
		return;

	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	OptionsTree* o = PsiOptions::instance();

	if (d->gbSelectedPreset->isEnabled())
	{
		statusPresetRejected();
		switchPresetMode(false);
	}
	//Unselect items
	d->lw_presets->setCurrentRow(-1);

	//Delete all presets
	foreach(QString key, o->getChildOptionNames("options.status.presets", true, true)) {
		o->removeOption(key, true);
	}

	//Recreate presets considering order from list widget
	for (int i = 0; i < d->lw_presets->count(); i++)
	{
		QListWidgetItem* item = d->lw_presets->item(i);
		StatusPreset sp = presets[item->text()];
		sp.filterStatus();
		sp.toOptions(o);
	}

	o->setOption("options.status.ask-for-message-on-online", d->ck_askOnline->isChecked());
	o->setOption("options.status.ask-for-message-on-chat", d->ck_askChat->isChecked());
	o->setOption("options.status.ask-for-message-on-away", d->ck_askAway->isChecked());
	o->setOption("options.status.ask-for-message-on-xa", d->ck_askXa->isChecked());
	o->setOption("options.status.ask-for-message-on-dnd", d->ck_askDnd->isChecked());
	o->setOption("options.status.ask-for-message-on-offline", d->ck_askOffline->isChecked());

	o->setOption("options.status.presets-in-status-menus", d->cb_presetsMenus->itemData(d->cb_presetsMenus->currentIndex()).toString());

	o->setOption("options.status.show-only-online-offline", d->ck_onlyOnOff->isChecked());
	o->setOption("options.status.show-choose", d->ck_showChoose->isChecked());
	o->setOption("options.status.show-reconnect", d->ck_showReconnect->isChecked());
	o->setOption("options.status.show-edit-presets", d->ck_showEditPresets->isChecked());
}

void OptionsTabStatusGeneral::restoreOptions()
{
	if ( !w )
		return;

	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	OptionsTree* o = PsiOptions::instance();

	//TODO: Restore function is calling 3 times! Do something with it! Or maybe it's normal?
	d->lw_presets->clear();
	presets.clear();

	foreach(QVariant name, o->mapKeyList("options.status.presets", true)) {
		StatusPreset sp;
		sp.fromOptions(o, name.toString());
		sp.filterStatus();

		presets[name.toString()] = sp;
#ifdef Q_OS_MAC
		QListWidgetItem* item = new QListWidgetItem(sp.name(), d->lw_presets);
#else
		QListWidgetItem* item = new QListWidgetItem(PsiIconset::instance()->status(sp.status()).icon(), sp.name(), d->lw_presets);
#endif
		d->lw_presets->addItem(item);
	}

	cleanupSelectedPresetGroup();
	switchPresetMode(false);

	d->ck_askOnline->setChecked( o->getOption("options.status.ask-for-message-on-online").toBool() );
	d->ck_askChat->setChecked( o->getOption("options.status.ask-for-message-on-chat").toBool() );
	d->ck_askAway->setChecked( o->getOption("options.status.ask-for-message-on-away").toBool() );
	d->ck_askXa->setChecked( o->getOption("options.status.ask-for-message-on-xa").toBool() );
	d->ck_askDnd->setChecked( o->getOption("options.status.ask-for-message-on-dnd").toBool() );
	d->ck_askOffline->setChecked( o->getOption("options.status.ask-for-message-on-offline").toBool() );

	d->cb_presetsMenus->setItemData(0, "submenu");
	d->cb_presetsMenus->setItemData(1, "yes");
	d->cb_presetsMenus->setItemData(2, "no");
	int mode = d->cb_presetsMenus->findData(o->getOption("options.status.presets-in-status-menus").toString());
	d->cb_presetsMenus->setCurrentIndex(mode == -1 ? 0 : mode);

	d->ck_onlyOnOff->setChecked( o->getOption("options.status.show-only-online-offline").toBool() );
	d->ck_showChoose->setChecked( o->getOption("options.status.show-choose").toBool() );
	d->ck_showReconnect->setChecked( o->getOption("options.status.show-reconnect").toBool() );
	d->ck_showEditPresets->setChecked( o->getOption("options.status.show-edit-presets").toBool() );
}

void OptionsTabStatusGeneral::setData(PsiCon *, QWidget *parentDialog)
{
	parentWidget = parentDialog;
}

void OptionsTabStatusGeneral::currentItemChanged(QListWidgetItem * current, QListWidgetItem * /*previous*/ )
{
	OptStatusGeneralUI *d = static_cast<OptStatusGeneralUI*>(w);
	d->pb_spEdit->setEnabled(current != 0);
	d->pb_spDelete->setEnabled(current != 0);
	if (current)
		loadStatusPreset();
	else
		cleanupSelectedPresetGroup();
}

void OptionsTabStatusGeneral::loadStatusPreset()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	QListWidgetItem* item = d->lw_presets->currentItem();
	if (!item)
		return;

	StatusPreset preset = presets[item->text()];
	preset.filterStatus();
	d->le_spName->setText(preset.name());
	d->le_spName->home(false);
	d->te_sp->setPlainText(preset.message());
	if (preset.priority().hasValue())
		d->le_sp_priority->setText(QString::number(preset.priority().value()));
	else
		d->le_sp_priority->clear();
	d->cb_sp_status->setStatus(preset.status());
}

void OptionsTabStatusGeneral::saveStatusPreset()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	QListWidgetItem* item = d->lw_presets->currentItem();
	if (!item)
		return;

	QString oldName = item->text();

	StatusPreset sp;
	sp.setName(d->le_spName->text());
	sp.setMessage(d->te_sp->toPlainText());
	sp.setPriority(d->le_sp_priority->text());
	sp.setStatus(d->cb_sp_status->status());
	sp.filterStatus();
	if (oldName != sp.name())
	{
		presets.remove(oldName);
		item->setText(sp.name());
	}
#ifndef Q_OS_MAC
	item->setIcon(PsiIconset::instance()->status(sp.status()).icon());
#endif
	presets[sp.name()] = sp;
}

void OptionsTabStatusGeneral::newStatusPreset()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	presets[""] = StatusPreset();
	d->lw_presets->addItem("");
	d->lw_presets->setCurrentRow(d->lw_presets->count()-1);

	loadStatusPreset();

	switchPresetMode(true);
}

void OptionsTabStatusGeneral::deleteStatusPreset()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	int current = d->lw_presets->currentRow();
	if (current == -1)
		return;

	QListWidgetItem* item = d->lw_presets->takeItem(current);
	presets.remove(item->text());

	// select a new entry if possible
	if(d->lw_presets->count() > 0) {
		d->lw_presets->setCurrentRow(d->lw_presets->count()-1);
	}
	else
		cleanupSelectedPresetGroup();

	//Emit dataChanged only if we delete existing item and not cancelling creating of new one
	if (!item->text().isEmpty())
		emit dataChanged();

	delete item;
}

void OptionsTabStatusGeneral::statusMenusIndexChanged ( int index )
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	if (index == -1 || d->cb_presetsMenus->itemData(d->cb_presetsMenus->currentIndex()).toString() == "no")
		d->ck_showEditPresets->setEnabled(false);
	else
		d->ck_showEditPresets->setEnabled(true);
}

void OptionsTabStatusGeneral::showMenuForPreset(const QPoint &point)
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	QListWidgetItem* item = d->lw_presets->itemAt(point);
	if (item)
	{
		//paranoia: we must be sure item is selected
		d->lw_presets->setCurrentItem(item);

		//Show menu
		spContextMenu->exec(d->lw_presets->mapToGlobal(point));
	}
}

void OptionsTabStatusGeneral::editStatusPreset()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	int current = d->lw_presets->currentRow();
	if (current == -1)
		return;

	loadStatusPreset();

	switchPresetMode(true);
}

void OptionsTabStatusGeneral::cleanupSelectedPresetGroup()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	d->le_spName->setText("");
	d->te_sp->setPlainText("");
	d->le_sp_priority->setText("");
	d->cb_sp_status->setStatus(XMPP::Status::Online);
}

void OptionsTabStatusGeneral::statusPresetAccepted()
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;
	QListWidgetItem* item = d->lw_presets->currentItem();
	if (!item)
		return;

	QString newName = d->le_spName->text();
	if(newName.trimmed().isEmpty())
	{
		QMessageBox::critical(parentWidget, tr("Error"), tr("Can't create a blank preset!"));
	}
	else if(newName != item->text() && presets.contains(newName))
	{
		QMessageBox::critical(parentWidget, tr("Error"), tr("You already have a preset with that name!"));
	}
	else
	{
		saveStatusPreset();
		switchPresetMode(false);
		loadStatusPreset();
		emit dataChanged();
	}
}

void OptionsTabStatusGeneral::statusPresetRejected()
{
	//TODO almost all functions have this line, GET RID OF IT
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	switchPresetMode(false);
	QListWidgetItem* item = d->lw_presets->currentItem();
	if (!item)
		return;

	if (item->text().isEmpty())
	{
		//Cancel creating new item
		deleteStatusPreset();
	}
	else
	{
		//Cancel editing, just refresh Selected Preset group for viewing
		loadStatusPreset();
	}
}

void OptionsTabStatusGeneral::switchPresetMode(bool toEdit)
{
	OptStatusGeneralUI *d = (OptStatusGeneralUI *)w;

	d->gbSelectedPreset->setEnabled(toEdit);
	d->gbPresets->setEnabled(!toEdit);
	d->gbStatusMenus->setEnabled(!toEdit);
	d->gbPrompts->setEnabled(!toEdit);

	emit enableDlgCommonWidgets(!toEdit);
}

void OptionsTabStatusGeneral::presetDoubleClicked(const QModelIndex & /*index*/ )
{
	editStatusPreset();
}
