/*
 * opt_shortcuts.cpp - an OptionsTab for setting the Keyboard Shortcuts of Psi
 * Copyright (C) 2006 Cestonaro Thilo
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

#include "opt_shortcuts.h"

#include <QMessageBox>

#include "common.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "grepshortcutkeydialog.h"

#include "ui_opt_shortcuts.h"

#define ITEMKIND Qt::UserRole
#define OPTIONSTREEPATH Qt::UserRole + 1

/**
 * \class the Ui for the Options Tab Shortcuts
 */
class OptShortcutsUI : public QWidget, public Ui::OptShortcuts
{
public:
	OptShortcutsUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabShortcuts
//----------------------------------------------------------------------------

/**
 * \brief Constructor of the Options Tab Shortcuts Class
 */
OptionsTabShortcuts::OptionsTabShortcuts(QObject *parent)
: OptionsTab(parent, "shortcuts", "", tr("Shortcuts"), tr("Options for Psi Shortcuts"), "psi/shortcuts")
{
	w = 0;
}

/**
 * \brief Destructor of the Options Tab Shortcuts Class
 */
OptionsTabShortcuts::~OptionsTabShortcuts()
{
}

/**
 * \brief widget, creates the Options Tab Shortcuts Widget
 * \return QWidget*, points to the previously created widget
 */
QWidget *OptionsTabShortcuts::widget()
{
	if ( w )
		return 0;

	w = new OptShortcutsUI();
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	
	d->treeShortcuts->setColumnWidth(0, 320);	

	d->add->setEnabled(false);
	d->remove->setEnabled(false);
	d->edit->setEnabled(false);

	connect(d->treeShortcuts, SIGNAL(itemSelectionChanged()), this, SLOT(onItemSelectionChanged()));
	connect(d->treeShortcuts, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(onItemDoubleClicked(QTreeWidgetItem *, int)));
	connect(d->add, SIGNAL(clicked()), this, SLOT(onAdd()));
	connect(d->remove, SIGNAL(clicked()), this, SLOT(onRemove()));
	connect(d->edit, SIGNAL(clicked()), this, SLOT(onEdit()));
	connect(d->restoreDefaults, SIGNAL(clicked()), this, SLOT(onRestoreDefaults()));
	return w;
}

/**
 * \brief	applyOptions, if options have changed, they will be applied by calling this function
 * \param	opt, unused, totally ignored
 */
void OptionsTabShortcuts::applyOptions() {
	if ( !w )
		return;

	OptShortcutsUI *d = (OptShortcutsUI *)w;
	PsiOptions *options = PsiOptions::instance();

	int toplevelItemsCount = d->treeShortcuts->topLevelItemCount();
	int shortcutItemsCount;
	int keyItemsCount;
	QTreeWidgetItem *topLevelItem;
	QTreeWidgetItem *shortcutItem;
	QTreeWidgetItem *keyItem;
	QString optionsPath;
	QString comment;
	QList<QString> children;
	QList<QKeySequence> keys;

	/* step through the Toplevel Items */
	for(int topLevelIndex = 0 ; topLevelIndex < toplevelItemsCount; topLevelIndex++) {
		topLevelItem = d->treeShortcuts->topLevelItem(topLevelIndex);
		shortcutItemsCount = topLevelItem->childCount();

		/* step through the Shortcut Items */
		for(int shortcutItemIndex = 0; shortcutItemIndex < shortcutItemsCount; shortcutItemIndex++) {
			shortcutItem = topLevelItem->child(shortcutItemIndex);
			keyItemsCount = shortcutItem->childCount();
			
			/* get the Options Path of the Shortcut Item */
			optionsPath = shortcutItem->data(0, OPTIONSTREEPATH).toString();

			/* just one Key Sequence */
			if(keyItemsCount == 1) {
				/* so set the option to this keysequence directly */
				keyItem = shortcutItem->child(0);
				options->setOption(optionsPath, QVariant(keyItem->text(1)));
			}
			else if(keyItemsCount > 1){
				/* more than one, then collect them in a list */
				QList<QVariant> keySequences;
				for(int keyItemIndex = 0; keyItemIndex < keyItemsCount; keyItemIndex++) {
					keyItem = shortcutItem->child(keyItemIndex);
					keySequences.append(QVariant(keyItem->text(1)));
				}
				
				options->setOption(optionsPath, QVariant(keySequences));
			}
			else {
				/* zero key sequences, so set an empty string, so it will be written empty to the options.xml */
				options->setOption(optionsPath, "");
			}
		}
	}
}

/**
 * \brief	restoreOptions, reads in the currently set options
 */
void OptionsTabShortcuts::restoreOptions()
{
	if ( !w )
		return;
	
	readShortcuts(PsiOptions::instance());
}

/**
 * \brief	readShortcuts, reads shortcuts from given PsiOptions instance
 */
void OptionsTabShortcuts::readShortcuts(const PsiOptions *options)
{
	OptShortcutsUI *d = (OptShortcutsUI *)w;

	QTreeWidgetItem *topLevelItem;
	QList<QString> shortcutGroups = options->getChildOptionNames("options.shortcuts", true, true);

	/* step through the shortcut groups e.g. chatdlg */
	foreach(QString shortcutGroup, shortcutGroups) {
		topLevelItem = new QTreeWidgetItem(d->treeShortcuts);

		QString comment = options->getComment(shortcutGroup);
		if (comment.isNull()) {
			comment = tr("Unnamed group");
		}
		else {
			comment = translateShortcut(comment);
		}
		topLevelItem->setText(0, comment);
		topLevelItem->setData(0, OPTIONSTREEPATH, QVariant(shortcutGroup));
		topLevelItem->setData(0, ITEMKIND, QVariant((int)OptionsTabShortcuts::TopLevelItem));
		topLevelItem->setExpanded(true);
		d->treeShortcuts->addTopLevelItem(topLevelItem);
	}

	int toplevelItemsCount = d->treeShortcuts->topLevelItemCount();
	QTreeWidgetItem *shortcutItem;
	QTreeWidgetItem *keyItem;
	QString optionsPath;
	QString comment;
	QList<QString> shortcuts;
	QList<QKeySequence> keys;
	int keyItemsCount;

	/* step through the toplevel items */
	for(int toplevelItemIndex = 0 ; toplevelItemIndex < toplevelItemsCount; toplevelItemIndex++) {
		topLevelItem = d->treeShortcuts->topLevelItem(toplevelItemIndex);
		optionsPath = topLevelItem->data(0, OPTIONSTREEPATH).toString();

		/* if a optionsPath was saved in the toplevel item, we can get the shortcuts and the keys for the shortcuts */
		if(!optionsPath.isEmpty()) {
			
			shortcuts = options->getChildOptionNames(optionsPath, true, true);
			/* step through the shortcuts */
			foreach(QString shortcut, shortcuts) {
				
				keys = ShortcutManager::readShortcutsFromOptions(shortcut.mid(QString("options.shortcuts").length() + 1), options);
				comment = options->getComment(shortcut);
				if (comment.isNull()) {
					comment = tr("Unnamed group");
				}
				else {
					comment = translateShortcut(comment);
				}
				
				/* create the TreeWidgetItem and set the Data the Kind and it's Optionspath and append it */
				shortcutItem = new QTreeWidgetItem(topLevelItem);
				shortcutItem->setText(0, comment);
				shortcutItem->setData(0, ITEMKIND, QVariant((int)OptionsTabShortcuts::ShortcutItem));
				shortcutItem->setData(0, OPTIONSTREEPATH, QVariant(shortcut));
				topLevelItem->addChild(shortcutItem);

				/* step through this shortcut's keys and create 'Key XXXX' entries for them */
				keyItemsCount = 1;
				foreach(QKeySequence key, keys) {
					keyItem = new QTreeWidgetItem(shortcutItem);
					keyItem->setText(0, QString(tr("Key %1")).arg(keyItemsCount++));
					keyItem->setData(0, ITEMKIND, QVariant((int)OptionsTabShortcuts::KeyItem));
					keyItem->setText(1, key.toString(QKeySequence::NativeText));
					shortcutItem->addChild(keyItem);
				}
			}
		}
	}
}

/**
 * \brief	Button Add pressed, creates a new Key entry
 */
void OptionsTabShortcuts::onAdd() {
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	
	QTreeWidgetItem	*shortcutItem;

	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();
	QString	optionsPath;
	Kind itemKind;

	if(selectedItems.count() == 0)
		return;

	shortcutItem = selectedItems[0];
	itemKind = (Kind)shortcutItem->data(0, ITEMKIND).toInt();

	switch(itemKind) {
		case OptionsTabShortcuts::KeyItem:
			/* it was a keyItem, so get it's parent */
			shortcutItem = shortcutItem->parent();
			break;	
		case OptionsTabShortcuts::ShortcutItem:
			break;
		default:
			return;
	}

	addTo(shortcutItem);
}

/**
 * \brief Adds a new key to given shortcut item
 * \param shortcutItem, new key parent, must be a ShortcutItem
 */
void OptionsTabShortcuts::addTo(QTreeWidgetItem *shortcutItem)
{
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();

	QTreeWidgetItem *newKeyItem = new QTreeWidgetItem(shortcutItem);
	newKeyItem->setText(0, QString(tr("Key %1")).arg(shortcutItem->childCount()));
	newKeyItem->setData(0, ITEMKIND, QVariant(OptionsTabShortcuts::KeyItem));
	shortcutItem->addChild(newKeyItem);
	if (selectedItems.count())
		selectedItems[0]->setSelected(false);
	newKeyItem->setExpanded(true);
	newKeyItem->setSelected(true);

	grep();
}

/**
 * \brief Button Remove pressed, removes the currently selected item, if it is a Keyitem
 */
void OptionsTabShortcuts::onRemove() {
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();

	if(selectedItems.count() == 0)
		return;

	QTreeWidgetItem	*shortcutItem;
	QTreeWidgetItem	*keyItem;
	int keyItemsCount;
	
	keyItem = selectedItems[0];

	/* we need a Item with the Kind "KeyItem", else we could / should not remove it */
	if((Kind)keyItem->data(0, ITEMKIND).toInt() == OptionsTabShortcuts::KeyItem) {
		shortcutItem = keyItem->parent();
		/* remove the key item from the shortcut item's children */
		shortcutItem->takeChild(shortcutItem->indexOfChild(keyItem));

		/* rename the children which are left over */
		keyItemsCount = shortcutItem->childCount();
		for( int keyItemIndex = 0; keyItemIndex < keyItemsCount; keyItemIndex++)
			shortcutItem->child(keyItemIndex)->setText(0, QString(tr("Key %1")).arg(keyItemIndex + 1));

		/* notify the options dlg that data was changed */
		emit dataChanged();	
	}
}

/**
 * \brief Button Edit pressed, edits the currently selected item if it is a key
 */
void OptionsTabShortcuts::onEdit() {
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();

	if(selectedItems.count() == 0)
		return;

	QTreeWidgetItem	*keyItem = selectedItems[0];

	if((Kind)keyItem->data(0, ITEMKIND).toInt() == OptionsTabShortcuts::KeyItem)
		grep();
}

/**
 * \brief Button Restore Defaults pressed
 */
void OptionsTabShortcuts::onRestoreDefaults() {
	if (QMessageBox::information(w, CAP(tr("Restore default shortcuts")),
                   tr("Are you sure you would like to restore the default shortcuts?"),
                   QMessageBox::Yes | QMessageBox::No,
                   QMessageBox::No) == QMessageBox::Yes) {

		OptShortcutsUI *d = (OptShortcutsUI *)w;
		d->treeShortcuts->clear();
		readShortcuts(PsiOptions::defaults());
		emit dataChanged();	
	}
}

/**
 * \brief Opens grep dialog.
 */
void OptionsTabShortcuts::grep()
{
	GrepShortcutKeyDialog* grep = new GrepShortcutKeyDialog();
	connect(grep, SIGNAL(newShortcutKey(const QKeySequence&)), this, SLOT(onNewShortcutKey(const QKeySequence&)));
	grep->show();
}

/**
 * \brief	in the treeview, the selected item has changed, the add and remove buttons are
 * 			enabled or disabled, depening on the selected item type
 */
void OptionsTabShortcuts::onItemSelectionChanged() {
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();
	Kind itemKind;

	/* zero selected Item(s), so we can't add or remove anything, disable the buttons */
	if(selectedItems.count() == 0) {
		d->add->setEnabled(false);
		d->remove->setEnabled(false);
		d->edit->setEnabled(false);
		return;
	}

	itemKind = (Kind)selectedItems[0]->data(0, ITEMKIND).toInt();
	switch(itemKind) {
		case OptionsTabShortcuts::TopLevelItem:
			/* for a topLevel Item, we can't do anything neither add a key, nor remove one */
			d->add->setEnabled(false);
			d->remove->setEnabled(false);
			d->edit->setEnabled(false);
			break;
		case OptionsTabShortcuts::ShortcutItem:
			/* at a shortcut Item, we can add a key, but not remove it */
			d->add->setEnabled(true);
			d->remove->setEnabled(false);
			d->edit->setEnabled(false);
			break;
		case OptionsTabShortcuts::KeyItem:
			/* at a key item, we can add a key to it's parent shortcut item, or remove it */
			d->add->setEnabled(true);
			d->remove->setEnabled(true);
			d->edit->setEnabled(true);
			break;
	}
}

/**
 * \brief	an item of the treeview is double clicked, if it is a Keyitem, the GrepShortcutKeyDialog is shown
 */
void OptionsTabShortcuts::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);

	if (!item)
		return;

 	Kind itemKind = (Kind)item->data(0, ITEMKIND).toInt();
	if (itemKind == KeyItem)
		grep();
	else if (itemKind == ShortcutItem && item->childCount() == 0)
		addTo(item);
}

/**
 * \brief	slot onNewShortcutKey is called when the grepShortcutKeyDialog has captured a KeySequence,
 *			so the new KeySequence can be set to the KeyItem
 * \param	the new KeySequence for the keyitem
 */
void OptionsTabShortcuts::onNewShortcutKey(const QKeySequence& key) {
	OptShortcutsUI *d = (OptShortcutsUI *)w;
	QTreeWidgetItem	*keyItem;
	QList<QTreeWidgetItem *> selectedItems = d->treeShortcuts->selectedItems();
	QString	optionsPath;
	Kind itemKind;

	if(selectedItems.count() == 0)
		return;

	keyItem = selectedItems[0];
	itemKind = (OptionsTabShortcuts::Kind)keyItem->data(0, ITEMKIND).toInt();

	/* if we got a key item, set the new key sequence and notify the options dialog that data has changed */
	if(itemKind == OptionsTabShortcuts::KeyItem) {
		keyItem->setText(1, key.toString(QKeySequence::NativeText));
		emit dataChanged();	
	}
}

/**
 * \brief	Translate the \param comment in the "Shortcuts" translation Context
 * \param	comment the text to be translated
 */
QString OptionsTabShortcuts::translateShortcut(QString comment)
{
	return QCoreApplication::translate("Shortcuts", comment.toLatin1());
}
