/*
 * actionlist.h - the customizeable action list
 * Copyright (C) 2004  Michail Pishchagin
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

#ifndef ACTIONLIST_H
#define ACTIONLIST_H

#include <QObject>
#include <QList>

class QString;
class QStringList;
class IconAction;

class ActionList : public QObject
{
	Q_OBJECT
public:
	ActionList(QString name, int id, bool autoDelete = true);
	ActionList(const ActionList &);
	~ActionList();

	QString name() const;
	int id() const;

	IconAction *action( QString name ) const;
	QStringList actions() const;

	void addAction( QString name, IconAction *action );

	void clear();

public:
	class Private;
private:
	Private *d;
};

class MetaActionList : public QObject
{
	Q_OBJECT
public:
	MetaActionList();
	~MetaActionList();

	ActionList *actionList( QString name ) const;
	QList<ActionList*> actionLists( int id ) const;
	QStringList actionLists() const;

	ActionList suitableActions( int id ) const;

	void addList( ActionList * );

	void clear();

private:
	class Private;
	Private *d;
};

#endif
