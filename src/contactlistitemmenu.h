/*
 * contactlistitemmenu.h - base class for contact list item context menus
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

#ifndef CONTACTLISTITEMMENU_H
#define CONTACTLISTITEMMENU_H

#include <QMenu>
#include <QList>

class ContactListModel;
class ContactListItem;
class QAction;

class ContactListItemMenu : public QMenu
{
	Q_OBJECT
public:
	ContactListItemMenu(ContactListItem* item, ContactListModel* model);
	virtual ~ContactListItemMenu();

	virtual ContactListItem* item() const;

	virtual void removeActions(QStringList actionNames);
	virtual QList<QAction*> availableActions() const;

protected:
	QKeySequence shortcut(const QString& name) const;
	QList<QKeySequence> shortcuts(const QString& name) const;
	ContactListModel* model() const;

private:
	ContactListItem* item_;
	ContactListModel* model_;
};

#endif
