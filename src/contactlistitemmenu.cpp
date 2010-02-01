/*
 * contactlistitemmenu.cpp - base class for contact list item context menus
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "contactlistitemmenu.h"

#include "shortcutmanager.h"

ContactListItemMenu::ContactListItemMenu(ContactListItem* item, ContactListModel* model)
	: QMenu(0)
	, item_(item)
	, model_(model)
{
}

ContactListItemMenu::~ContactListItemMenu()
{
}

ContactListItem* ContactListItemMenu::item() const
{
	return item_;
}

/**
 * Removes all actions which objectNames are present in \param actionNames.
 */
void ContactListItemMenu::removeActions(QStringList actionNames)
{
	foreach(QString actionName, actionNames) {
		foreach(QAction* action, actions()) {
			if (action->objectName() == actionName) {
				delete action;
				break;
			}
		}
	}
}

QList<QAction*> ContactListItemMenu::availableActions() const
{
	QList<QAction*> result;
	foreach(QAction* action, actions())
		if (!action->isSeparator())
			result << action;
	return result;
}

QList<QKeySequence> ContactListItemMenu::shortcuts(const QString& name) const
{
	return ShortcutManager::instance()->shortcuts(name);
}

QKeySequence ContactListItemMenu::shortcut(const QString& name) const
{
	return ShortcutManager::instance()->shortcut(name);
}

ContactListModel* ContactListItemMenu::model() const
{
	return model_;
}
