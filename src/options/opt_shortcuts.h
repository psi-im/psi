/*
 *
 * opt_shortcuts.h - an OptionsTab for setting the Keyboard Shortcuts of Psi
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

#ifndef OPT_SHORTCUTS_H
#define OPT_SHORTCUTS_H

#include "optionstab.h"
#include <QKeySequence>

class QTreeWidgetItem;
class QWidget;

class OptionsTabShortcuts : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabShortcuts(QObject *parent);
	~OptionsTabShortcuts();

	QWidget *widget();
	void applyOptions(Options *opt);
	void restoreOptions(const Options *opt);

	typedef enum Kind {
		TopLevelItem = 1,
		ShortcutItem,
		KeyItem
	};

private slots:
	void onAdd();
	void onRemove();
	void onEdit();
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onItemSelectionChanged();
	void onNewShortcutKey(QKeySequence key);

private:
	void addTo(QTreeWidgetItem *shortcutItem);
	void grep();
	QString translateShortcut(QString comment);

	QWidget *w;
};

#endif
