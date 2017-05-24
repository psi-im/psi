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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactlistitemmenu.h"

#include "shortcutmanager.h"
#include "psioptions.h"

#include <QWidgetAction>
#include <QLabel>

ContactListItemMenu::ContactListItemMenu(ContactListItem* item, ContactListModel* model)
	: QMenu(0)
	, item_(item)
	, model_(model)
	, _lblTitle(nullptr)
{
	const QString css = PsiOptions::instance()->getOption("options.ui.contactlist.css").toString();
	if (!css.isEmpty()) {
		setStyleSheet(css);
	}

	if (PsiOptions::instance()->getOption("options.ui.contactlist.menu-titles", false).toBool()) {
		_lblTitle = new QLabel;
		QPalette palette = _lblTitle->palette();
		QColor textColor = palette.color(QPalette::BrightText);
		QColor bcgColor = palette.color(QPalette::Dark);
		QFont font = _lblTitle->font();
		font.setBold(true);
		palette.setColor(QPalette::WindowText, textColor);
		palette.setColor(QPalette::Window, bcgColor);
		_lblTitle->setPalette(palette);
		_lblTitle->setAutoFillBackground(true);
		_lblTitle->setAlignment(Qt::AlignCenter);
		_lblTitle->setMargin(6);
		_lblTitle->setFont(font);

		QWidgetAction *waContextMenuTitle = new QWidgetAction(this);
		waContextMenuTitle->setDefaultWidget(_lblTitle);
		addAction(waContextMenuTitle);
	}
}

ContactListItemMenu::~ContactListItemMenu()
{
}

ContactListItem* ContactListItemMenu::item() const
{
	return item_;
}

void ContactListItemMenu::setLabelTitle(const QString &title)
{
	if (_lblTitle) {
		_lblTitle->setText(title);
	}
}

/**
 * Removes all actions which objectNames are present in \param actionNames.
 */
void ContactListItemMenu::removeActions(QStringList actionNames)
{
	for (const QString &actionName: actionNames) {
		for (QAction *action: actions()) {
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
	for (QAction* action: actions())
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
